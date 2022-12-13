This component provides a helper that allows a server process to handle multiple
concurrent IPCs coming through a `NamedPlatformChannel`.

## Example usage

In the server process:
```cpp
static const uint64_t kMessagePipeId = 0u;

class MyInterfaceImpl: public mojom::MyInterface {
  void Start() {
    server_.set_disconnect_handler(
        base::BindRepeating(&MyInterfaceImpl::OnDisconnected, this));
    server_.StartServer();
  }

  void OnDisconnected() {
    LOG(INFO) << "Receiver disconnected: " << server_.current_receiver();
  }

  // mojom::MyInterface Implementation.
  void DoWork() override {
    // Do something...

    // If you want to close the connection:
    server_.Close(server_.current_receiver());
  }

  NamedMojoIpcServer<mojom::MyInterface> server_{
      {
        .server_name = "my_server_name",
        .message_pipe_id = kMessagePipeId,
        // Other options when necessary...
      }, this,
      base::BindRepeating([](mojom::MyInterface* impl, base::ProcessId caller) {
        // Verify the calling process, returning nullptr if unverified.
        return impl; // impl must outlive NamedMojoIpcServer.
      }, this)};
};
```

Note: In unittests `base::test:TaskEnvironment` should run until idle after
`NamedMojoIpcServer` is shutdown. Otherwise, memory may leak. E.g:

```cpp
void MyTestFixture::TearDown() {
   ipc_server_->StopServer();
   task_environment_.RunUntilIdle();
 }
```

In the client:
```cpp
void ConnectToServer() {
mojo::PlatformChannelEndpoint endpoint =
      named_mojo_ipc_server::ConnectToServer(server_name);
auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
mojo::Remote<mojom::MyInterface> remote(
  mojo::PendingRemote<mojom::MyInterface>(
    invitation.ExtractMessagePipe(kMessagePipeId), 0));
}
```

Note that for compatibility with all supported platforms clients should use
`named_mojo_ipc_server::ConnectToServer` instead of
`mojo::NamedPlatformChannel::ConnectToServer`. Some platforms require
additional connection brokerage steps which are abstracted by the former.

On Windows, the server needs to have the following access rights on the client
process: `PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION`.

It is possible to bind a different implementation of the interface to each
connection by returning different `mojom::MyInterface*` values rather than
`this`. All implementations must outlive the NamedMojoIpcServer.
