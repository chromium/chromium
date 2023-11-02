// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/scoped_xpc_service_mock.h"

namespace updater {

ScopedXPCServiceMock::ScopedXPCServiceMock(Protocol* service_protocol)
    : service_protocol_(service_protocol) {
  @autoreleasepool {
    // Each NSXPCConnection mock must re-mock alloc (when a new mock is created
    // for type T, it un-mocks all previous classmethod mocks on type T - this
    // behavior differs from the documentation). Add a stub for alloc that
    // calls GTEST_FAIL(), since the only way this alloc stub is reachable
    // is if it has not been replaced with an alloc stub from a prepared
    // connection mock.
    nsxpcconnection_class_mock_.reset(OCMClassMock([NSXPCConnection class]),
                                      base::scoped_policy::RETAIN);
    OCMStub(ClassMethod([nsxpcconnection_class_mock_.get() alloc]))
        .andDo(^(NSInvocation* invocation) {
          GTEST_FAIL() << "[NSXPCConnection alloc] called before any "
                          "connection mocks prepared";
        });
  }
}

ScopedXPCServiceMock::~ScopedXPCServiceMock() = default;

void ScopedXPCServiceMock::HandleConnectionAlloc(NSInvocation* invocation) {
  ASSERT_TRUE(invocation);

  size_t conn_idx = next_connection_to_vend_++;
  ASSERT_GT(mocked_connections_.size(), conn_idx);

  id mock_connection_ptr = mocked_connections_[conn_idx]->Get();
  // alloc returns net retain count of 1; OCMock doesn't automatically do this
  // when using .andDo, only when using .andReturn. Retain the return value here
  // to provide this net RC 1 behavior.
  [mock_connection_ptr retain];
  // NSInvocation copies the value referenced by the pointer provided to
  // -setReturnValue (in this case, a pointer itself).
  [invocation setReturnValue:&mock_connection_ptr];
}

ScopedXPCServiceMock::RemoteObjectMockRecord::RemoteObjectMockRecord(
    base::scoped_nsprotocol<id> mock_ptr)
    : mock_object(mock_ptr) {}

ScopedXPCServiceMock::RemoteObjectMockRecord::~RemoteObjectMockRecord() =
    default;

ScopedXPCServiceMock::ConnectionMockRecord::ConnectionMockRecord(
    ScopedXPCServiceMock* mock_driver,
    size_t index)
    : service_protocol_(mock_driver->service_protocol_),
      index_(index),
      mock_connection_(OCMClassMock([NSXPCConnection class]),
                       base::scoped_policy::RETAIN) {
  @autoreleasepool {
    id mock_connection = mock_connection_.get();  // local for convenience

    // Every time we create a new mock of type X, all class method stubs for
    // type X are dropped. This is different from the behavior stated
    // in OCMock documentation. As a workaround for this behavior, we must
    // recreate the +[NSXPCConnection alloc] stub every time we create
    // a new mock for NSXPCConnection.
    //
    // If an OCMock update breaks these tests in mysterious ways, this
    // workaround has probably outlived its usefulness.
    OCMStub(ClassMethod([mock_connection alloc]))
        .andDo(^(NSInvocation* invocation) {
          mock_driver->HandleConnectionAlloc(invocation);
        });

    // Expect this connection to be initialized with some service name.
    OCMExpect([mock_connection initWithMachServiceName:[OCMArg any] options:0])
        .andReturn(mock_connection);

    // Expect this connection to receive a correct declaration of the remote
    // interface: an NSXPCInterface configured with the protocol provided
    // when |this| was constructed.
    id verifyRemoteInterface = [OCMArg checkWithBlock:^BOOL(id interfaceArg) {
      if (!interfaceArg)
        return NO;
      if (![interfaceArg isKindOfClass:[NSXPCInterface class]])
        return NO;
      NSXPCInterface* interface = interfaceArg;
      // This test does not verify that the interface is declared correctly
      // around methods that require proxying or collection whitelisting.
      // Use end-to-end tests to detect this.
      return [interface.protocol isEqual:service_protocol_];
    }];
    OCMExpect([mock_connection setRemoteObjectInterface:verifyRemoteInterface]);

    // When remote objects are requested from this connection, they will come
    // from conn_rec_ptr->remote_object_mocks. If an error handler is
    // provided, we must populate the corresponding RemoteObjectMockRecord's
    // xpc_error_handler field. If we have run out of mock remote objects,
    // we must create a new one (just like if we run out of connections).
    OCMStub([mock_connection remoteObjectProxyWithErrorHandler:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          ASSERT_TRUE(invocation);
          ASSERT_LT(next_mock_to_vend_, remote_object_mocks_.size());
          RemoteObjectMockRecord* const mock_rec_ptr =
              remote_object_mocks_[next_mock_to_vend_++].get();
          id block_ptr;
          // Extract the error handler block. Objective-C Runtime argument
          // numbering convention starts with 0=self and 1=_cmd (the selector
          // for the current invocation); index 2 is the first "standard"
          // argument. NSInvocation is not type safe and it is very difficult to
          // derive the type of a block, so we'll just have to rely on
          // EXC_BAD_ACCESS if something that isn't even a block goes here.
          [invocation getArgument:&block_ptr atIndex:2];
          mock_rec_ptr->xpc_error_handler =
              base::mac::ScopedBlock<void (^)(NSError*)>(block_ptr);
          // Copy the mock remote object pointer so it has an address, since
          // -[NSInvocation setReturnValue:] requires indirection.
          id mock_remote_object_ptr = mock_rec_ptr->mock_object.get();
          [invocation setReturnValue:&mock_remote_object_ptr];
        });
    OCMStub([mock_connection remoteObjectProxy])
        .andDo(^(NSInvocation* invocation) {
          ASSERT_TRUE(invocation);
          ASSERT_LT(next_mock_to_vend_, remote_object_mocks_.size());
          RemoteObjectMockRecord* const mock_rec_ptr =
              remote_object_mocks_[next_mock_to_vend_++].get();
          // Copy the mock remote object pointer so it has an address, since
          // -[NSInvocation setReturnValue:] requires indirection.
          id mock_remote_object_ptr = mock_rec_ptr->mock_object.get();
          [invocation setReturnValue:&mock_remote_object_ptr];
        });

    // We don't expect to suspend connections, but we do have to resume them.
    OCMStub([(NSXPCConnection*)mock_connection suspend])
        .andDo(^(NSInvocation*) {
          GTEST_FAIL() << "suspend not implemented on mock NSXPCConnection.";
        });
    OCMExpect([(NSXPCConnection*)mock_connection resume]);

    // An NSXPCConnection must be invalidated before it is deallocated. Test
    // implementors take note: if this expectation fails, check the lifespan
    // of your object under test. If it's still alive during validation, this
    // will fail because the object hasn't cleaned up its connections yet.
    OCMExpect([mock_connection invalidate]);
  }  // autoreleasepool
}  // ScopedXPCServiceMock::ConnectionMockRecord constructor

ScopedXPCServiceMock::ConnectionMockRecord::~ConnectionMockRecord() {
  [mock_connection_ stopMocking];
}

id ScopedXPCServiceMock::ConnectionMockRecord::Get() {
  return mock_connection_.get();
}

ScopedXPCServiceMock::ConnectionMockRecord*
ScopedXPCServiceMock::PrepareNewMockConnection() {
  mocked_connections_.push_back(
      std::make_unique<ConnectionMockRecord>(this, mocked_connections_.size()));
  return mocked_connections_.back().get();
}

void ScopedXPCServiceMock::VerifyAll() {
  for (const auto& connection_ptr : mocked_connections_) {
    connection_ptr->Verify();
  }
}

size_t ScopedXPCServiceMock::PreparedConnectionsCount() const {
  return mocked_connections_.size();
}

size_t ScopedXPCServiceMock::VendedConnectionsCount() const {
  return next_connection_to_vend_;
}

ScopedXPCServiceMock::ConnectionMockRecord* ScopedXPCServiceMock::GetConnection(
    size_t idx) {
  if (idx >= mocked_connections_.size()) {
    return nullptr;
  }
  return mocked_connections_[idx].get();
}

const ScopedXPCServiceMock::ConnectionMockRecord*
ScopedXPCServiceMock::GetConnection(size_t idx) const {
  if (idx >= mocked_connections_.size()) {
    return nullptr;
  }
  return const_cast<const ConnectionMockRecord*>(
      mocked_connections_[idx].get());
}

size_t ScopedXPCServiceMock::ConnectionMockRecord::PreparedObjectsCount()
    const {
  return remote_object_mocks_.size();
}

size_t ScopedXPCServiceMock::ConnectionMockRecord::VendedObjectsCount() const {
  return next_mock_to_vend_;
}

size_t ScopedXPCServiceMock::ConnectionMockRecord::Index() const {
  return index_;
}

ScopedXPCServiceMock::RemoteObjectMockRecord*
ScopedXPCServiceMock::ConnectionMockRecord::PrepareNewMockRemoteObject() {
  base::scoped_nsprotocol<id> mock(OCMProtocolMock(service_protocol_),
                                   base::scoped_policy::RETAIN);
  remote_object_mocks_.push_back(
      std::make_unique<RemoteObjectMockRecord>(mock));
  return remote_object_mocks_.back().get();
}

ScopedXPCServiceMock::RemoteObjectMockRecord*
ScopedXPCServiceMock::ConnectionMockRecord::GetRemoteObject(
    size_t object_index) {
  if (object_index >= remote_object_mocks_.size()) {
    return nullptr;
  }
  return remote_object_mocks_[object_index].get();
}

const ScopedXPCServiceMock::RemoteObjectMockRecord*
ScopedXPCServiceMock::ConnectionMockRecord::GetRemoteObject(
    size_t object_index) const {
  if (object_index >= remote_object_mocks_.size()) {
    return nullptr;
  }
  return const_cast<const RemoteObjectMockRecord*>(
      remote_object_mocks_[object_index].get());
}

void ScopedXPCServiceMock::ConnectionMockRecord::Verify() const {
  EXPECT_OCMOCK_VERIFY(mock_connection_.get())
      << "Verification failure in connection " << index_;
  for (size_t idx = 0; idx < remote_object_mocks_.size(); ++idx) {
    EXPECT_OCMOCK_VERIFY(remote_object_mocks_[idx]->mock_object.get())
        << "Verification failure in object " << idx << " on connection "
        << index_;
  }
}

}  // namespace updater
