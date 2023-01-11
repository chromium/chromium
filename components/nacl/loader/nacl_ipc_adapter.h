// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_IPC_ADAPTER_H_
#define COMPONENTS_NACL_LOADER_NACL_IPC_ADAPTER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/pickle.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/proxy/nacl_message_scanner.h"

struct NaClDesc;
struct NaClImcTypedMsgHdr;

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class Channel;
struct ChannelHandle;
}

// Adapts a Chrome IPC channel to an IPC channel that we expose to Native
// Client. This provides a mapping in both directions, so when IPC messages
// come in from another process, we rewrite them and allow them to be received
// via a recvmsg-like interface in the NaCl code. When NaCl code calls sendmsg,
// we implement that as sending IPC messages on the channel.
//
// This object also provides the necessary logic for rewriting IPC messages.
// NaCl code is platform-independent and runs in a Posix-like enviroment, but
// some formatting in the message and the way handles are transferred varies
// by platform. This class bridges that gap to provide what looks like a
// normal platform-specific IPC implementation to Chrome, and a Posix-like
// version on every platform to NaCl.
//
// This object must be threadsafe since the nacl environment determines which
// thread every function is called on.
class NaClIPCAdapter : public base::RefCountedThreadSafe<NaClIPCAdapter>,
                       public IPC::Listener {
 public:
  // Chrome's IPC message format varies by platform, NaCl's does not. In
  // particular, the header has some extra fields on Posix platforms. Since
  // NaCl is a Posix environment, it gets that version of the header. This
  // header is duplicated here so we have a cross-platform definition of the
  // header we're exposing to NaCl.
#pragma pack(push, 4)
  struct NaClMessageHeader : public base::Pickle::Header {
    int32_t routing;
    uint32_t type;
    uint32_t flags;
    uint16_t num_fds;
    uint16_t pad;
  };
#pragma pack(pop)

  typedef base::OnceCallback<void(IPC::PlatformFileForTransit, base::FilePath)>
      ResolveFileTokenReplyCallback;

  typedef base::RepeatingCallback<void(uint64_t,  // file_token_lo
                                       uint64_t,  // file_token_hi
                                       ResolveFileTokenReplyCallback)>
      ResolveFileTokenCallback;

  typedef base::OnceCallback<
      void(const IPC::Message&, IPC::PlatformFileForTransit, base::FilePath)>
      OpenResourceReplyCallback;

  typedef base::RepeatingCallback<bool(const IPC::Message&,
                                       const std::string&,  // key
                                       OpenResourceReplyCallback)>
      OpenResourceCallback;

  // Creates an adapter, using the thread associated with the given task
  // runner for posting messages. In normal use, the task runner will post to
  // the I/O thread of the process.
  //
  // If you use this constructor, you MUST call ConnectChannel after the
  // NaClIPCAdapter is constructed, or the NaClIPCAdapter's channel will not be
  // connected.
  //
  // |resolve_file_token_cb| is an optional callback to be invoked for
  // resolving file tokens received from the renderer. When the file token
  // is resolved, the ResolveFileTokenReplyCallback passed inside the
  // ResolveFileTokenCallback will be invoked. |open_resource_cb| is also an
  // optional callback to be invoked for intercepting open_resource IRT calls.
  // |open_resource_cb| may immediately call a OpenResourceReplyCallback
  // function to send a pre-opened resource descriptor to the untrusted side.
  // OpenResourceCallback returns true when OpenResourceReplyCallback is called.
  NaClIPCAdapter(const IPC::ChannelHandle& handle,
                 const scoped_refptr<base::SingleThreadTaskRunner>& runner,
                 ResolveFileTokenCallback resolve_file_token_cb,
                 OpenResourceCallback open_resource_cb);

  // Initializes with a given channel that's already created for testing
  // purposes. This function will take ownership of the given channel.
  NaClIPCAdapter(std::unique_ptr<IPC::Channel> channel,
                 base::TaskRunner* runner);

  NaClIPCAdapter(const NaClIPCAdapter&) = delete;
  NaClIPCAdapter& operator=(const NaClIPCAdapter&) = delete;

  // Connect the channel. This must be called after the constructor that accepts
  // an IPC::ChannelHandle, and causes the Channel to be connected on the IO
  // thread.
  void ConnectChannel();

  // Implementation of sendmsg. Returns the number of bytes written or -1 on
  // failure.
  int Send(const NaClImcTypedMsgHdr* msg);

  // Implementation of recvmsg. Returns the number of bytes read or -1 on
  // failure. This will block until there's an error or there is data to
  // read.
  int BlockingReceive(NaClImcTypedMsgHdr* msg);

  // Closes the IPC channel.
  void CloseChannel();

  // Make a NaClDesc that refers to this NaClIPCAdapter. Note that the returned
  // NaClDesc is reference-counted, and a reference is returned.
  NaClDesc* MakeNaClDesc();

  // Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

 private:
  friend class base::RefCountedThreadSafe<NaClIPCAdapter>;

  class RewrittenMessage;

  // This is the data that must only be accessed inside the lock. This struct
  // just separates it so it's easier to see.
  struct LockedData {
    LockedData();
    ~LockedData();

    // Messages that we have read off of the Chrome IPC channel that are waiting
    // to be received by the plugin.
    base::queue<std::unique_ptr<RewrittenMessage>> to_be_received_;

    ppapi::proxy::NaClMessageScanner nacl_msg_scanner_;

    // Data that we've queued from the plugin to send, but doesn't consist of a
    // full message yet. The calling code can break apart the message into
    // smaller pieces, and we need to send the message to the other process in
    // one chunk.
    //
    // The IPC channel always starts a new send() at the beginning of each
    // message, so we don't need to worry about arbitrary message boundaries.
    std::string to_be_sent_;

    bool channel_closed_;
  };

  // This is the data that must only be accessed on the I/O thread (as defined
  // by TaskRunner). This struct just separates it so it's easier to see.
  struct IOThreadData {
    IOThreadData();
    ~IOThreadData();

    std::unique_ptr<IPC::Channel> channel_;

    // When we send a synchronous message (from untrusted to trusted), we store
    // its type here, so that later we can associate the reply with its type
    // for scanning.
    typedef std::map<int, uint32_t> PendingSyncMsgMap;
    PendingSyncMsgMap pending_sync_msgs_;
  };

  ~NaClIPCAdapter() override;

  void SaveOpenResourceMessage(const IPC::Message& orig_msg,
                               IPC::PlatformFileForTransit ipc_fd,
                               base::FilePath file_path);

  bool RewriteMessage(const IPC::Message& msg, uint32_t type);

  // Returns 0 if nothing is waiting.
  int LockedReceive(NaClImcTypedMsgHdr* msg);

  // Sends a message that we know has been completed to the Chrome process.
  bool SendCompleteMessage(const char* buffer, size_t buffer_len);

  // Clears the LockedData.to_be_sent_ structure in a way to make sure that
  // the memory is deleted. std::string can sometimes hold onto the buffer
  // for future use which we don't want.
  void ClearToBeSent();

  void ConnectChannelOnIOThread();
  void CloseChannelOnIOThread();
  void SendMessageOnIOThread(std::unique_ptr<IPC::Message> message);

  // Saves the message to forward to NaCl. This method assumes that the caller
  // holds the lock for locked_data_.
  void SaveMessage(const IPC::Message& message,
                   std::unique_ptr<RewrittenMessage> rewritten_message);

  base::Lock lock_;
  base::ConditionVariable cond_var_;

  scoped_refptr<base::TaskRunner> task_runner_;

  ResolveFileTokenCallback resolve_file_token_cb_;
  OpenResourceCallback open_resource_cb_;

  // To be accessed inside of lock_ only.
  LockedData locked_data_;

  // To be accessed on the I/O thread (via task runner) only.
  IOThreadData io_thread_data_;
};

// Export TranslatePepperFileReadWriteOpenFlags for testing.
int TranslatePepperFileReadWriteOpenFlagsForTesting(int32_t pp_open_flags);

#endif  // COMPONENTS_NACL_LOADER_NACL_IPC_ADAPTER_H_
