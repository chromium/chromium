# Mojo scheduling examples

This directory contains three Mojo examples that are more advanced than the one
in `//codelabs/cpp101/`. They demonstrate how Mojo works in a simplified,
multi-process environment *outside* of Chromium code to help demystify how it
all works and show:
 - What is going on under the hood when Chromium launches a new child process
   that communicates with its parent via Mojo
 - How associated interfaces are registered/requested

This examples demonstrate how the [Mojo Invitations API][1] works, as well as
some complexities and observable differences when it comes to different types of
associated interfaces. In particular, these examples demonstrate the ordering
guarantees that associated interfaces provide, and how these guarantees differ
between traditional associated interfaces and legacy IPC _channel-associated_
interfaces.

The three example subdirectories each contain two binaries that are toy
recreations of the "browser" and "renderer" processes, for demonstration
purposes. Each example directory is described below.

## 01-multi-process

This is a simple example demonstrating how to form a cross-process Mojo
connection with `//base` primitives like `base::Process`, and the [Mojo
Invitations API][1]. It contains two binaries called `01-mojo-browser` and
`01-mojo-renderer`. You can run the example with these commands:

```
$ autoninja -C out/<Directory> 01-mojo-browser 01-mojo-renderer
$ cd out/<Directory>
$ ./01-mojo-browser
```


## 02-associated-interface-freezing

This is a slightly more complicated example containing `02-mojo-browser` and
`02-mojo-renderer` binaries similar to before, but in this case the "browser"
process requests that the "renderer" bind two receivers for two different
interfaces that are mutually associated. The renderer binds each interface to
two different receivers associated with task runners belonging to two different
queues: the default one which never gets froze, and a freezable queue which
starts out "frozen".

Side-note: to understand how task queues can be scheduled independently, see the
examples in `//codelabs/threading_and_scheduling/02-task-queue-priorities.cc`.

The first associated IPC that the cross-process remote sends ends up on a
receiver bound to a frozen queue, while the second IPC ends up on a receiver
whose queue is not frozen. This example shows that despite each receiver's task
queues being scheduled independently, the messages get delivered in the order
that *they were sent* from the cross-process remote. This is a consequence of
Mojo internals ensuring that the ordering for associated interfaces is
guaranteed regardless of external scheduling circumstances.

You can run the example with these commands:

```
$ autoninja -C out/<Directory> 02-mojo-browser 02-mojo-renderer
$ cd out/<Directory>
$ ./02-mojo-browser
```


## 03-channel-associated-interface-freezing

The previous example demonstrates Mojo as the sole provider of the cross-process
connection, but in practice, Chromium's legacy IPC implementation is still used
in a number of places in the tree, and forms the primordial cross-process
connections between the browser ↔ renderer processes. As a result, Mojo
interfaces spanning these processes are said to run "on top of" the legacy IPC
system. This results in some observable implementation differences between "pure
Mojo" vs Mojo as it runs over existing legacy IPC channels.

One profound behavior difference between Mojo in these two contexts is how far
Mojo internals are willing to go to maintain ordering between associated
interfaces. The previous example demonstrated that Mojo internals guarantee
ordering mutually-associated interfaces, even if the associated receivers are
scheduled differently (or frozen, for example). This is true even if that means
significantly delaying the delivery of some messages, so that ordering can still
be maintained.

However, _channel-associated_ Mojo interfaces do not have this same guarantee;
messages bound for mutually-associated interfaces are scheduled to be
well-ordered as they come in on the IO thread, but their delivery does not block
whatsoever.

In practice, this means it's possible for channel-associated messages to get out
of order in a way that is not possible with "pure Mojo" associated messages:
  1. If said messages get dispatched to associated receivers, one of which has
     not been bound yet, subsequent messages _do not_ block on the delivery of
     the previous undelivered messages
  1. If a message comes in for a channel-associated receiver that's bound to a
     frozen task queue, the message _does not_ block and is delivered
     immediately, so that we minimize the number of messages that get delivered
     out of order (because remember, we don't block subsequent messages that
     come in).

This contrived example demonstrates the aforementioned properties of
channel-associated mojo interfaces. You can run the example with these commands:

```
$ autoninja -C out/<Directory> 03-mojo-browser 03-mojo-renderer
$ cd out/<Directory>
$ ./03-mojo-browser
```

## 04-legacy-ipc-with-separate-remote

This example is very similar to the previous example in that the legacy IPC
channel is setup but it creates an intermediate unassociated object. Anything
associated with the intermediate object then has the pure mojo scheduling
properties, as seen in example 02, not the legacy IPC ChannelProxy scheduling
properties.

[1]: https://chromium.googlesource.com/chromium/src/+/master/mojo/public/cpp/system/README.md#Invitations
