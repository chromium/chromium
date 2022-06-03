# Using the Scoped XPC Service Mock

*Adam Norberg -- norberg@google.com -- 2020-10-16*

[TOC]

# Overview

XPC is Apple's recommended technology for modern interprocess communication.
It is asynchronous and does not provide test hooks, so it is difficult to
test code that uses an XPC service. ScopedXPCServiceMock uses OCMock 3 to
mock the core behaviors of XPC to make it practical to write unit tests for
client code.

# OCMock

[OCMock 3](https://ocmock.org/reference/) is an engine for creating mock objects
for testing in Objective-C. A copy of version 3.1.5 is available in
[third_party/ocmock.](https://source.chromium.org/chromium/chromium/src/+/main:third_party/ocmock/)
Note that this is several versions behind the current public release of OCMock,
so some features described on OCMock's site are not present.

ScopedXPCServiceMock doesn't understand what your service is supposed to do, so
you will need to implement mock behavior for the service yourself, using the
mock objects that ScopedXPCServiceMock creates and makes available to you.
The XPC subsystem itself, however, is mocked out for you.

## Helpers in ScopedXPCServiceMock

OCMockBlockCapturer and OCMockObjectCapturer are helper classes for capturing
arguments provided to mocked-out calls. The captured block or object can then
be used or inspected as required by the remainder of the test. This is probably
the easiest way to capture objects or blocks used as callbacks, so they can
be invoked at a later point in the test sequence.

OCMock has a built-in feature for invoking a captured block immediately, but
does not assist in setting up a delayed invocation. OCMockBlockCapturer is
intended for capturing blocks that will be invoked at some later point in the
test; this can be used to verify that, for example, a client does not proceed
with a task before it has received a callback from the server, since the time
before the callback is invoked is available for verifying test expectations.
Using OCMockObjectCapturer with an object that provides callbacks is similar.

Both classes function equivalently:

*  Instantiate an OCMockObjectCapturer or OCMockBlockCapturer in the test body
   for each distinct argument that is to be captured.
*  When stubbing out a method that takes as an argument an object or block you
   intend to capture, provide `capturer.Capture()` as the argument (where
   `capturer` is replaced with your capturer object). The `Capture()` method
   vends an OCMArg that will transfer arguments it observes to the Capturer
   that created it.
*  Run your test.
*  To retrieve objects or blocks captured with a Capturer, call `.Get()`, which
   returns (by const reference) a `std::vector` containing all objects that
   this specific capturer has observed, in order. Each call will create exactly
   entry in this vector, including calls that provide `nil` to this
   argument; these entries contain `nil`. `capturer.Get().size()` equals the
   number of times the method to which this capturer is attached has been
   invoked.

Capturers maintain strong references to captured arguments for their lifespans.
These references are released when the Capturer is destroyed.


# Core Abstractions

ScopedXPCServiceMock includes types to represent mocked connections and the
exported objects served over those connections. Because XPC mocking requires
class methods (NSXPCConnection's alloc and init behaviors), only one root
mock can exist; however, that root can represent a sequence of connections,
which can in turn issue a sequence of mock remote objects. Test cases that
establish multiple connections or make multiple calls to `.remoteObjectProxy`
(or `remoteObjectProxyWithErrorHandler:`) within a connection receive _distinct_
mocks for each call.

ScopedXPCServiceMock constructs ConnectionMockRecord instances, which are used
to configure the behavior of each mocked NSXPCConnection created. These mocks
are served in the same order they are created. Similarly, ConnectionMockRecord
creates RemoteObjectMockRecord instances that include the specific mock objects
that the mock connection appears to serve. Test code is responsible for adding
stubs to these mocks to perform the actions required for the test; the
block and object capturer helpers (see above) are intended for use when
creating these stubs.

## ScopedXPCServiceMock

ScopedXPCServiceMock mocks out NSXPCConnection as long as it exists, and cleans
up these mocks (returning NSXPCConnection to normal operation) when it is
destroyed. Multiple instances of ScopedXPCServiceMock cannot coexist. The
effects of ScopedXPCServiceMock are process-wide, not thread-bound, so tests
involving ScopedXPCServiceMock cannot be parallelized.

The most common pattern for using ScopedXPCServiceMock is:

1. Allocate the ScopedXPCServiceMock at the start of the test.
2. For each time a connection will be created during the test, call
   `PrepareNewMockConnection`, and use the returned ConnectionMockRecord to
   further configure test behaviors for this connection, including the behavior
   of mock remote objects that the mock connection will vend. (See below.)
3. Run the test scenario to completion.
4. Call `VerifyAll`, which will cause the test to fail if any mocked connection
   was not vended, any mocked remote object on any mocked connection was not
   vended, or any expectations on the mocked remote objects are unment. In test
   scenarios that are not expected to engage in these behaviors, `VerifyAll`
   cannot be used; more granular verification is available on the individual
   ConnectionMockRecord and RemoteObjectMockRecord instances.
5. Delete the ScopedXPCServiceMock (typically by falling out of scope).

Allocating ScopedXPCServiceMock starts the override of NSXPCConnection, and
deallocating it cleans up. Attempting to create NSXPCConnections that have not
been prepared with corresponding calls to PrepareNewMockConnection will
intentionally crash.

## ConnectionMockRecord

ConnectionMockRecord represents the metadata associated with a mock connection.
The ScopedXPCServiceMock library takes care of emulating most NSXPCConnection
behaviors, so direct access to the underlying mock object is not expected to
be necessary, nor is it recommended. ConnectionMockRecord must only be
constructed by ScopedXPCServiceMock, inside calls to `PrepareNewMockConnection`.

Each ConnectionMockRecord has an index, which represents the order it was
created in and will be vended in. (Indexes start at 0.) This is constant
during the life of the ConnectionMockRecord and can be used as an ID to fetch
or uniquely identify the ConnectionMockRecord relative to a ScopedXPCServiceMock
instance.

Just like ScopedXPCServiceMock creates mock connections it will vend,
ConnectionMockRecord creates RemoteObjectMockRecord instances representing
remote objects the mock will vend. `PrepareNewMockRemoteObject` creates
such a mock, and increments the `PreparedObjectsCount`. When the code under
test requests a remote object proxy, and the mock connection vends a mock
remote object, `VendedObjectsCount` is incremented.

`Verify` expects that the connection was initialized and torn down correctly,
expects that every mock remote object was vended, and checks expectations on
mock objects that were vended.

ConnectionMockRecord does not provide facilities to emulate connection-wide
interruption or invalidation handlers. Attempts to assign to them crash.
If this toolset is to be used to test code that assigns to these handlers,
additional code must be written for ConnectionMockRecord.

## RemoteObjectMockRecord

RemoteObjectMockRecord is the narrowest mock abstraction provided by
ScopedXPCServiceMock. It is a `struct` because it is a straightforward container
associating a mock object with the error handler provided by the code under
test when the mock was vended.

Use the `mock_object` field to configure the behavior of the mocked remote
object. This object is an OCMock 3 `OCMProtocolMock` initialized with the
protocol provided when ScopedXPCServiceMock itself was constructed.

`xpc_error_handler` is not intended to be written directly by the test code.
It is populated when the mock remote object is vended due to a call to
`remoteObjectProxyWithErrorHandler:`, and it stores the provided error
handler. If the object is vended due to `.remoteObjectProxy`, or no actual
error handler is provided to `remoteObjectProxyWithErrorHandler`, this field
will remain `nullopt`. This field is intended to be used for simulating
XPC connection failures or other XPC errors.

RemoteObjectMockRecord is a struct, so its fields are intended for direct use.

# Step by step through a test case

`update_service_out_of_process_test.mm` contains `SimpleProductUpdate`, a test
that simulates the process of updating one product. It uses a helper object
created for update tests, StateChangeTestEngine, to simulate callbacks from
the update engine itself and to check that the XPC service correctly reports
those callbacks, translated to Objective-C, later. This document is intended
to focus on uses of ScopedXPCServiceMock, so it will not go into detail on
StateChangeTestEngine.

This unit test uses the MacUpdateServiceOutOfProcessTest test fixture, which
creates a ScopedXPCServiceMock named `mock_driver_` when allocated, along with
a `base::test::SingleThreadTaskEnvironment` to allow Chromium sequence behavior
to be intercepted and manipulated for testing. In `SetUp`, it prepares a
reference to the active RunLoop, and prepares to define `service_` later - a
field that will only be filled once the runloop starts running, because
allocating an UpdateServiceOutOfProcess causes it to initialize an
NSXPCConnection - so we must not allocate the service itself until after
we have prepared a suitable mock connection and the test is generally ready
for operation.

This test only dials one XPC connection, from which exactly one remote
object will be requested. This is likely to be fairly common. The test first
specifies one connection that should vend one mock object, and saves borrowed
mutable pointers to the connection record (`conn_rec`), the mock record
(`mock_rec`), and the mock object itself (`mock_remote_object`). Note that
this code is compiled without ARC, so `mock_remote_object` is also a borrowed
pointer (the running code does not own the reference; the lifespan of the
referent is managed by some other object). These pointers will remain valid
for the lifespan of `mock_driver_`, which is longer than the entire scope
in which these pointers exist, so no other memory management is required.

`UpdateService::Update` has _two_ output parameters: in addition to the
typical XPC reply block as the last argument, the real service accepts a
CRUUpdateStateObserving callback to provide intermediate status callbacks
as the update is processed. The XPC service definition explicitly defines
this argument as a proxy, and CRUUpdateStateObserving is itself an XPC-ready
protocol to permit this cross-process callback. Since the test is using mocks
to simulate service behavior, the test needs to capture the objects that will
be used for asynchronous callbacks. `reply_block_capturer` and
`update_state_observer_capturer` will be used for this purpose.

Next, a StateChangeTestEngine is prepared, using a bunch of helper functions
that make the code more legible at the cost of obscuring what's happening here.
A StateChangeTestEngine provides a sequence of calls to a CRUUpdateStateObserver
but also provides a C++ callback that expects to see the states it provided
in Objective-C form translated to C++. To avoid recreating the translation logic
in the tests, the StateChangeTestEngine cannot calculate this expectation
itself; instead, it is initialized with a list of _pairs_ of states to use:
the Objective-C state to provide when emulating the service and the
corresponding C++ state to expect when checking its output. These states are
verbose, so constructing these pairs of states is encapsulated in helper
functions. (Repeating all these states manually would be both illegible and
error-prone.) This engine is intending to emulate an update check followed
by download and successful installation of a single product.

There are now enough pieces in place to define the behavior of the mock object
that will pretened to be the remote XPC proxy. The behavior is specified in
an Objective-C block; however, blocks create const copies of variables they
capture, so we need to explicitly use _pointers_ to the reply block capturer,
the update state observer, and the state change engine, because we do not want
copies of these objects (which are uncopyable types anyway) - we want to feed
data from the test back into them. Once copied into the block, these variables
are constant pointers into mutable objects - the "const-ness" of the copied
variable is applied to the copied variable itself, which is the pointer.

We are simulating a single update of a single product, so the mock object
expects exactly one call, into the method that checks for updates for one
product. It expects to see the app ID and priority we specified earlier. It also
expects to receive a callback object and a completion block; we use our capturer
helper objects to retain these arguments for later use. Note that preparing the
captures does not require the extra pointers we created, since it's not inside
a block. However, the `.andDo` macro does take a block, so we use the pointers
when specifying the details of how the test should handle this call.

The block provided here hands off most of the test execution itself to
the StateChangeTestEngine. It pulls out the captured callback and reply
arguments and provides them to `StartSimulating`. We expect exactly one call
to this method, so we use the 0th captured item from each capturer.
StartSimulating wants a 0-argument "when done" callback, so we bind the reply
we intend to eventually give to the reply block when handing it off.

After that, the call to be tested is itself enqueued. `state_change_engine`
provides a progress callback through `Watch` much the same way the `Capture`
methods work on the OCMock item capturer helpers. `Update` has its own
completion callback; this one checks that we got the success result we expected
and then tells the test runloop to stop.

With the setup and test operations queued and ready, the test now runs. All
of this behavior is mediated through callbacks, so control doesn't return to
the test method until the runloop stops. (Note that test runloops have a built
in timeout, so we don't need to add anything else to make sure the test
eventually halts if something goes wrong with our callback dance, as it did
many times during development of this test - spotting a real bug along the way.)

`run_loop_->Run` doesn't return until the runloop stops, so we follow up by
checking observable "final effects" of this call. StateChangeTestEngine already
verified all our partial progress callbacks, and the completion callback sent
to `service_->Update` already verified the result code, so here we check that
we saw exactly one request for a connection, one request for an object on
that connection, and one captured reply block. These checks are somewhat
redundant with the later `mock_driver_.VerifyAll` call; only the reply block
capturer count is not explicitly checked (again) during that call.

`VerifyAll` expects the connection to have been correctly shut down by the time
it is invoked, so the service must explicitly be disposed of before verification
commences.

## Discussion

Test cases for other sequences of update check behavior would look broadly
similar. Testing a no-update case, or a case where installation fails, could
copy the entire test and change only the state pairs used to construct the
StateChangeTestEngine. Testing XPC errors would require more development
work, since StateChangeTestEngine does not (as of 2020-OCT-21) help with this;
this would involve retrieving the error callback from the RemoteObjectMockRecord
after the object has been vended and invoking it with a realistic XPC error.

This test is verbose, as most of these tests will be, because
ScopedXPCServiceMock is designed specifically to emulate Apple's XPC behaviors
but has no opinion on what the client or server should actually _do_ with the
XPC connection. The test therefore must still specify all service behaviors
to mock and perform work to get the client running on an appropriate sequence,
then verify that the mock service replies were interpreted as intended.