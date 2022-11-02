# Named Mojo IPC Server

This component provides a helper that uses a mojo::NamedPlatformChannel to
manage multiple concurrent IPCs. Clients that connect to the
NamedPlatformChannel are sent invitations to join an isolated IPC graph
suitable only for direct IPC between the two processes.

## Caveats

The isolated connections managed by NamedMojoIpcServer can only be used to
connect two nodes which have been initialized as Mojo brokers. That is,
the `is_broker_process` field in the configuration passed to `mojo::core::Init`
must be set to true. This assumes that these isolated connections effectively
only go between two standalone (i.e. non-Chrome) processes, or between some
standalone process and the browser process; but never between two existing
Chrome processes, or between a standalone process and one of Chrome's child
processes.

A restriction of isolated connections is that Mojo remotes, receivers, or
message pipes cannot be passed outside of the boundary of the process pairs.
For example, the server cannot be used broker connections between two clients.

## Example usage

In the server process:
```cpp
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

  MojoIpcServer<mojom::MyInterface> server_{"my_server_name", this};
};
```

In the client:
```cpp
void ConnectToServer() {
mojo::PlatformChannelEndpoint endpoint =
  mojo::NamedPlatformChannel::ConnectToServer("my_server_name");

std::unique_ptr<mojo::IsolatedConnection> connection =
    std::make_unique<mojo::IsolatedConnection>();

mojo::Remote<mojom::MyInterface> remote(
  mojo::PendingRemote<mojom::MyInterface>(
    connection->Connect(std::move(endpoint)), 0));
}
```
