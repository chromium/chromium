# Elevated Tracing Service

[TOC]

## About

The elevated tracing service is a Windows service that consumes system-wide ETW
events and produces corresponding perfetto trace events.

## Installation

The tracing service is not registered for use by default. It is supported only
on per-machine (system-level) installs of Chrome. The service can be installed
manually by running one of the following commands (depending on which channel of
Chrome is installed) from an elevated cmd prompt:

- `"%ProgramFiles%\Google\Chrome Dev\Application\W.X.Y.Z\Installer\setup.exe" --chrome-dev --system-level --enable-system-tracing`
- `"%ProgramFiles%\Google\Chrome Beta\Application\W.X.Y.Z\Installer\setup.exe" --chrome-beta --system-level --enable-system-tracing`
- `"%ProgramFiles%\Google\Chrome\Application\W.X.Y.Z\Installer\setup.exe" --system-level --enable-system-tracing`

In each case, replace `W.X.Y.Z` with the true version installed. Installation
persists across Chrome browser updates, so this command need only be run once on
a given computer. The service can be uninstalled by running the same command,
but with `--disable-system-tracing` in place of `--enable-system-tracing`.

### For developers

Developers can enable the tracing service for a local build by running the
following command from an elevated cmd prompt: `out\Default\setup.exe
--developer --enable-system-tracing`. As above, use `--developer
--disable-system-tracing` to uninstall the service.

*** note
**Caution:** This will likely break tracing for a normal installation of the
same browser (e.g., stable Google Chrome if running a branded build), and may be
overwritten by an update of the same browser.
***

## Process lifecycle

The service process is started automatically when Chrome creates an instance of
the `SystemTracingSession` COM class. Clients are refused an instance of the
class if one is already active in the service. The service terminates when
either the active instance is destroyed (on account of its client releasing all
references) or if the client process terminates. Chrome is expected to hold a
reference to the session instance for as long as ETW trace events are desired.

## Session activation

A session becomes active via a multi-step negotiation.

1. Chrome creates an instance of `SystemTracingSession`.
2. Chrome creates a named mojo channel to establish a connection with the
   service and passes its name to the session's `AcceptInvitation()` method.
3. The service begins monitoring the client (Chrome) for termination, connects
   to the named channel to accept the invitation from it, and returns its PID to
   the client.
4. Chrome sends a mojo invitation with an initial message pipe (named `0`) over
   the channel to the service.
5. The service takes its end of the message pipe from the invitation, wraps it
   in a `PendingReceiver<tracing::mojom::TracedProcess>`, and passes it to the
   service's `tracing::TracedProcess` instance.
6. Chrome wraps its end of the initial message pipe in a `PendingRemote<...>`
   and passes it to the perfetto tracing service running in a utility process.
