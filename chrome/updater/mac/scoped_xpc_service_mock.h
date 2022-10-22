// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SCOPED_XPC_SERVICE_MOCK_H_
#define CHROME_UPDATER_MAC_SCOPED_XPC_SERVICE_MOCK_H_

#import <Foundation/Foundation.h>

#include "base/callback.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_policy.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace updater {
// ScopedXPCServiceMock sets up mocks for an arbitrary XPC service and
// provides access to those mocks. Mocks are removed when the object is
// deallocated. Only one of these objects can exist at a time, although
// it is not a singleton, since creating and destroying the mock manager
// is part of its behavior.
class ScopedXPCServiceMock {
 public:
  // RemoteObjectMockRecord represents a single call to a mock NSXPCConnection's
  // remoteObjectProxyWithErrorHandler: or remoteObjectProxy method.
  // It contains a reference to the mock created in reply to the call,
  // and the block provided as an error handler. If no block was provided
  // (including when .remoteObjectProxy is used, which cannot accept any
  // error handler block argument), xpc_error_handler will be nullopt.
  struct RemoteObjectMockRecord {
    // The mock object that will be served. Use this to configure behaviors
    // and expectations for the test.
    const base::scoped_nsprotocol<id> mock_object;

    // The error handler provided when the remote object was requested by the
    // code under test. Will not be populated before that request is issued.
    // If the remote object is requested via .remoteObjectProxy rather than
    // remoteObjectProxyWithErrorHandler:, this field is never populated.
    absl::optional<base::mac::ScopedBlock<void (^)(NSError*)>>
        xpc_error_handler;

    explicit RemoteObjectMockRecord(base::scoped_nsprotocol<id> mock_ptr);
    ~RemoteObjectMockRecord();
  };

  // ConnectionMockRecord encapsulates a mock NSXPCConnection and the objects
  // that will be served across that connection to consecutive calls.
  class ConnectionMockRecord {
   public:
    // Constructs a ConnectionMockRecord to be served in the specified order.
    // This constructor is not intended for use outside of
    // ScopedXPCServiceMock. It produces a mock connection that is ready
    // for use, but has no associated mock remote objects.
    // It requires a pointer to its enclosing ScopedXPCServiceMock to
    // maintain correct behavior of mocked alloc operations.
    ConnectionMockRecord(ScopedXPCServiceMock* mock_driver, size_t index);
    ~ConnectionMockRecord();

    ConnectionMockRecord(const ConnectionMockRecord& other) = delete;
    ConnectionMockRecord& operator=(const ConnectionMockRecord& other) = delete;
    ConnectionMockRecord(ConnectionMockRecord&& other) = delete;
    ConnectionMockRecord& operator=(ConnectionMockRecord&& other) = delete;

    // Gets the underlying connection mock itself (Objective-C flavor).
    // The mock is retained by |this|, and is not added to any autorelease
    // pools in response to this call.
    id Get();

    // Allocates a mock remote object, the next one to serve on this mocked
    // connection.
    //
    // Returns a pointer to the created RemoteObjectMockRecord, with |index| and
    // |mock_object| initialized. |xpc_error_handler| is guaranteed to be empty
    // when the record is created.
    //
    // The returned pointer is valid exactly as long as |this| is valid.
    RemoteObjectMockRecord* PrepareNewMockRemoteObject();

    // Find an already-created mock object by connection index.
    // Returns nullptr if no such index exists.
    //
    // The returned pointer is valid exactly as long as |this| is valid.
    RemoteObjectMockRecord* GetRemoteObject(size_t object_index);
    const RemoteObjectMockRecord* GetRemoteObject(size_t object_index) const;

    // Number of remote object mocks created for this mock connection.
    size_t PreparedObjectsCount() const;
    // Number of remote object mocks provided to test code from this mock
    // connection.
    size_t VendedObjectsCount() const;

    // Where in the order of connections that will be served by the enclosing
    // mock this mock connection is.
    size_t Index() const;

    // Verify that this connection was initialized and torn down properly,
    // and verifies OCMock expectations set on mock remote objects.
    void Verify() const;

   private:
    Protocol* service_protocol_;  // Copied from mock_driver
    // The index of |this| in ScopedXPCServiceMock::mocked_connections_.
    // Used in verification failure messages.
    const size_t index_;
    base::scoped_nsobject<id> mock_connection_;  // mock NSXPCConnection
    std::vector<std::unique_ptr<RemoteObjectMockRecord>> remote_object_mocks_;
    size_t next_mock_to_vend_ = 0;
  };  // class ConnectionMockRecord

  // Constructs a ScopedXPCServiceMock, stubbing out NSXPCConnection to create
  // mocks for connections to an XPC service fitting the provided protocol.
  // The mocks are cleaned up when the constructed object is destructed.
  explicit ScopedXPCServiceMock(Protocol* service_protocol);
  ~ScopedXPCServiceMock();

  ScopedXPCServiceMock(const ScopedXPCServiceMock&) = delete;
  ScopedXPCServiceMock& operator=(const ScopedXPCServiceMock&) = delete;
  ScopedXPCServiceMock(ScopedXPCServiceMock&&) = delete;
  ScopedXPCServiceMock& operator=(ScopedXPCServiceMock&&) = delete;

  // Prepares another mock connection to be served on a consecutive expected
  // call to +[NSXPCConnection alloc]. Returns the record for the new mock.
  ConnectionMockRecord* PrepareNewMockConnection();

  ConnectionMockRecord* GetConnection(size_t index);
  const ConnectionMockRecord* GetConnection(size_t index) const;

  // Number of connection mocks created.
  size_t PreparedConnectionsCount() const;
  // Number of connection mocks provided to test code.
  size_t VendedConnectionsCount() const;

  // Verify all expectations on all mocked connections and all remote objects
  // for those connections. Callers may find it useful to perform more detailed
  // verification of results beyond what OCMExpect can capture. Mocked
  // connections expect to be allocated, initialized, resumed, and invalidated.
  void VerifyAll();

 private:
  // Implement [NSXPCConnection alloc] during the mock, working with an
  // NSInvocation object. The call is expected to have 0 arguments. To provide
  // a return value for this invocation of alloc, alter |invocation|.
  //
  // This implementation constructs an NSXPCConnection prepared to yield a
  // sequence of mock remote objects as previously prepared by |this|, referring
  // to the next prepared mock connection in sequence. If not enough connections
  // have been prepared, this prepares one first.
  void HandleConnectionAlloc(NSInvocation* invocation);

  // What protocol do we expect to expose over the XPC interface?
  Protocol* service_protocol_;

  // All connection mocks we have currently created.
  std::vector<std::unique_ptr<ConnectionMockRecord>> mocked_connections_;
  size_t next_connection_to_vend_ = 0;

  base::scoped_nsobject<id> nsxpcconnection_class_mock_;
};  // class ScopedXPCServiceMock

#pragma mark - Helper classes

// OCMockBlockCapturer<B> stores blocks provided as arguments to a method
// mocked from OCMock. Due to limitations in objective-C, its type checking
// is not good, so if attempts to call blocks captured by this object
// crash, you probably got a block of an unexpected type.
template <typename B>
class OCMockBlockCapturer {
 public:
  OCMockBlockCapturer() = default;
  OCMockBlockCapturer(const OCMockBlockCapturer&) = delete;
  OCMockBlockCapturer& operator=(const OCMockBlockCapturer&) = delete;
  OCMockBlockCapturer(OCMockBlockCapturer&&) = default;
  OCMockBlockCapturer& operator=(OCMockBlockCapturer&&) = default;
  ~OCMockBlockCapturer() = default;

  // Returns the captured blocks, in capture order.
  const std::vector<base::mac::ScopedBlock<B>>& Get() const { return blocks_; }

  // Retrieves an OCMArg that will store blocks passed when the method is
  // invoked. The OCMArg matches any argument passed to the mock.
  id Capture() {
    if (!arg_.get()) {
      arg_.reset([OCMArg checkWithBlock:^BOOL(id value) {
                   blocks_.emplace_back(static_cast<B>(value),
                                        base::scoped_policy::RETAIN);
                   return YES;
                 }],
                 base::scoped_policy::RETAIN);
    }
    return (id)arg_.get();
  }

 private:
  std::vector<base::mac::ScopedBlock<B>> blocks_;
  base::scoped_nsobject<OCMArg> arg_;
};  // template class OCMockBlockCapturer

// OCMockObjectCapturer<NST> retains objects provided as arguments to a method
// mocked from OCMock. It rejects arguments that fail isKindOfClass checks.
template <typename NST>
class OCMockObjectCapturer {
 public:
  OCMockObjectCapturer() = default;
  OCMockObjectCapturer(const OCMockObjectCapturer&) = delete;
  OCMockObjectCapturer& operator=(const OCMockObjectCapturer&) = delete;
  OCMockObjectCapturer(OCMockObjectCapturer&&) = default;
  OCMockObjectCapturer& operator=(OCMockObjectCapturer&&) = default;
  ~OCMockObjectCapturer() = default;

  // Retrieves the captured objects, in capture order.
  const std::vector<base::scoped_nsobject<NST>>& Get() const {
    return objects_;
  }

  // Retrieves an OCMArg that will store blocks passed when the method is
  // invoked. The OCMArg matches arguments of the specified type.
  id Capture() {
    if (!arg_.get()) {
      arg_.reset([OCMArg checkWithBlock:^BOOL(id value) {
                   if (!value) {
                     objects_.push_back(base::scoped_nsobject<NST>{});
                     return YES;
                   }
                   if (![value isKindOfClass:[NST class]]) {
                     return NO;
                   }
                   objects_.emplace_back(static_cast<NST*>(value),
                                         base::scoped_policy::RETAIN);
                   return YES;
                 }],
                 base::scoped_policy::RETAIN);
    }
    return (id)arg_.get();
  }

 private:
  std::vector<base::scoped_nsobject<NST>> objects_;
  base::scoped_nsobject<OCMArg> arg_;
};  // template class OCMockObjectCapturer

#pragma mark - Helper functions

// A generic function template that ignores arguments and calls GTEST_FAIL.
// |msg| will be logged when this is called. Arguments are not printed (since
// unexpected values may not be loggable and may not exist).
template <typename... Ts>
void ExpectNoCalls(std::string msg, Ts... nope) {
  GTEST_FAIL() << msg;
}

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_SCOPED_XPC_SERVICE_MOCK_H_
