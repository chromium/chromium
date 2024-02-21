// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_TRANSLATE_THREAD_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_TRANSLATE_THREAD_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "components/nacl/renderer/plugin/plugin_error.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/serialized_handle.h"

struct PP_PNaClOptions;

namespace plugin {

class NaClSubprocess;
class PnaclCoordinator;

class PnaclTranslateThread {
 public:
  PnaclTranslateThread();

  PnaclTranslateThread(const PnaclTranslateThread&) = delete;
  PnaclTranslateThread& operator=(const PnaclTranslateThread&) = delete;

  ~PnaclTranslateThread();

  // Set up the state for RunCompile and RunLink. When an error is
  // encountered, or RunLink is complete the finish_callback is run
  // to notify the main thread.
  void SetupState(const pp::CompletionCallback& finish_callback,
                  NaClSubprocess* compiler_subprocess,
                  NaClSubprocess* ld_subprocess,
                  std::vector<base::File>* obj_files,
                  int num_threads,
                  base::File* nexe_file,
                  ErrorInfo* error_info,
                  PP_PNaClOptions* pnacl_options,
                  const std::string& architecture_attributes,
                  PnaclCoordinator* coordinator);

  // Create a compile thread and run/command the compiler_subprocess.
  // It will continue to run and consume data as it is passed in with PutBytes.
  // On success, runs compile_finished_callback.
  // On error, runs finish_callback.
  // The compiler_subprocess must already be loaded.
  void RunCompile(const pp::CompletionCallback& compile_finished_callback);

  // Create a link thread and run/command the ld_subprocess.
  // On completion (success or error), runs finish_callback.
  // The ld_subprocess must already be loaded.
  void RunLink();

  // Kill the llc and/or ld subprocesses. This happens by closing the command
  // channel on the plugin side, which causes the trusted code in the nexe to
  // exit, which will cause any pending SRPCs to error. Because this is called
  // on the main thread, the translation thread must not use the subprocess
  // objects without the lock, other than InvokeSrpcMethod, which does not
  // race with service runtime shutdown.
  void AbortSubprocesses();

  // Send bitcode bytes to the translator. Called from the main thread.
  void PutBytes(const void* data, int count);

  // Notify the translator that the end of the bitcode stream has been reached.
  // Called from the main thread.
  void EndStream();

  int64_t GetCompileTime() const { return compile_time_; }

  // Returns true if the translation process is initiated via SetupState.
  bool started() const { return !!coordinator_; }

 private:
  ppapi::proxy::SerializedHandle GetHandleForSubprocess(base::File* file,
                                                        int32_t open_flags);

  // Runs the streaming compilation. Called from the helper thread.
  void DoCompile();
  // Similar to DoCompile(), but for linking.
  void DoLink();

  class CompileThread : public base::SimpleThread {
   public:
    CompileThread(PnaclTranslateThread* obj)
      : base::SimpleThread("pnacl_compile"), pnacl_translate_thread_(obj) {}

    CompileThread(const CompileThread&) = delete;
    CompileThread& operator=(const CompileThread&) = delete;

   private:
    raw_ptr<PnaclTranslateThread> pnacl_translate_thread_;
    void Run() override;
  };

  class LinkThread : public base::SimpleThread {
   public:
    LinkThread(PnaclTranslateThread* obj)
      : base::SimpleThread("pnacl_link"), pnacl_translate_thread_(obj) {}

    LinkThread(const LinkThread&) = delete;
    LinkThread& operator=(const LinkThread&) = delete;

   private:
    raw_ptr<PnaclTranslateThread> pnacl_translate_thread_;
    void Run() override;
  };

  // Signal that Pnacl translation failed, from the translation thread only.
  void TranslateFailed(PP_NaClError err_code,
                       const std::string& error_string);

  // Callback to run when compile is completed and linking can start.
  pp::CompletionCallback compile_finished_callback_;

  // Callback to run when tasks are completed or an error has occurred.
  pp::CompletionCallback report_translate_finished_;

  std::unique_ptr<base::SimpleThread> translate_thread_;

  // Used to guard compiler_subprocess, ld_subprocess,
  // compiler_subprocess_active_, and ld_subprocess_active_
  // (touched by the main thread and the translate thread).
  base::Lock subprocess_mu_;
  // The compiler_subprocess and ld_subprocess memory is owned by the
  // coordinator so we do not delete them. However, the main thread delegates
  // shutdown to this thread, since this thread may still be accessing the
  // subprocesses. The *_subprocess_active flags indicate which subprocesses
  // are active to ensure the subprocesses don't get shutdown more than once.
  // The subprocess_mu_ must be held when shutting down the subprocesses
  // or otherwise accessing the service_runtime component of the subprocess.
  // There are some accesses to the subprocesses without locks held
  // (invoking srpc_client methods -- in contrast to using the service_runtime).
  raw_ptr<NaClSubprocess> compiler_subprocess_;
  raw_ptr<NaClSubprocess> ld_subprocess_;
  bool compiler_subprocess_active_;
  bool ld_subprocess_active_;

  // Mutex for buffer_cond_.
  base::Lock cond_mu_;
  // Condition variable to synchronize communication with the SRPC thread.
  // SRPC thread waits on this condvar if data_buffers_ is empty (meaning
  // there is no bitcode to send to the translator), and the main thread
  // appends to data_buffers_ and signals it when it receives bitcode.
  base::ConditionVariable buffer_cond_;
  // Data buffers from FileDownloader are enqueued here to pass from the
  // main thread to the SRPC thread. Protected by cond_mu_
  base::circular_deque<std::string> data_buffers_;
  // Whether all data has been downloaded and copied to translation thread.
  // Associated with buffer_cond_
  bool done_;

  int64_t compile_time_;

  // Data about the translation files, owned by the coordinator
  raw_ptr<std::vector<base::File>> obj_files_;
  int num_threads_;
  raw_ptr<base::File> nexe_file_;
  raw_ptr<ErrorInfo> coordinator_error_info_;
  raw_ptr<PP_PNaClOptions> pnacl_options_;
  std::string architecture_attributes_;
  raw_ptr<PnaclCoordinator> coordinator_;

  // These IPC::SyncChannels can only be used and freed by the parent thread.
  std::unique_ptr<IPC::SyncChannel> compiler_channel_;
  std::unique_ptr<IPC::SyncChannel> ld_channel_;
  // These IPC::SyncMessageFilters can be used by the child thread.
  scoped_refptr<IPC::SyncMessageFilter> compiler_channel_filter_;
  scoped_refptr<IPC::SyncMessageFilter> ld_channel_filter_;
};

}
#endif // COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_TRANSLATE_THREAD_H_
