# Chrome Cleaner Security Model

[TOC]

## Security Model

The purpose of the Chrome Cleanup Tool and Software Reporter Tool is to scan
files that are found on users' machines for the presence of Unwanted Software
(UwS) that could effect chrome. These files are untrusted inputs so, according
to the [Rule of
2](https://chromium.googlesource.com/chromium/src/+/main/docs/security/rule-of-2.md)
all code that parses them must run in a sandbox.

Sandboxes are implemented using the [chromium sandbox
library.](https://chromium.googlesource.com/chromium/src/+/main/docs/design/sandbox.md)

*** note
**TL;DR:** each sandbox runs in a separate process with severely reduced
privileges, called a "sandbox target" process. The main process has higher
privileges and is called the "broker" process.
***

The tool has separate sandboxes for each type of parsing that's done on
untrusted files:

*  [Engine
   Sandbox](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/engines/):
   The main scanning engine that detects UwS runs entirely in this sandbox.
*  [Parser Sandbox](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/parsers/):
   Parses various file formats such as JSON (mainly for Chrome extension settings) and Windows LNK files.
*  [Zip Archiver
   Sandbox](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/zip_archiver/):
   Compresses files into a .zip archive. Used when quarantining detected UwS
   files.

## Sandbox Infrastructure

The code for each sandbox is split into two subdirectories, `broker` and
`target`, for easier auditing.

*  `broker` contains code that runs in the broker process.
   *  This code has all the privileges of the user running the app.
   *  Code can tell that it's running in the broker process because
      [SandboxFactory::GetBrokerServices](https://source.chromium.org/chromium/chromium/src/+/main:sandbox/win/src/sandbox_factory.h)
      will return non-null.
*  `target` contains code that runs in the target process.
   *  This process is considered untrusted because malicious files could gain
      control of it by exploiting bugs in the parser.
   * This code has highly restricted privileges, including:
     *  Cannot access any files or registry keys that aren't explicitly shared
        by the broker process.
     *  Cannot access the network to exfiltrate data.
   *  Code can tell that it's running in the broker process because
      [SandboxFactory::GetTargetServices](https://source.chromium.org/chromium/chromium/src/+/main:sandbox/win/src/sandbox_factory.h)
      will return non-null.

The target process communicates with the broker process using
[Mojo](https://chromium.googlesource.com/chromium/src/+/main/mojo/README.md).

*  Mojo interfaces are found in the
   [mojom](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/mojom)
   directory.
*  A Mojo interface is a security boundary since it allows the untrusted target
   process to communicate with the broker process. This means:
   *  The interface should follow the Chromium security [Mojo Style
      Guide](https://chromium.googlesource.com/chromium/src/+/main/docs/security/mojo.md).
      Note that the style guide calls the "broker" process the "browser"
      process.
   *  All changes to the interface must be reviewed by an IPC security
      reviewer. This is enforced by the [OWNERS
      file](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/mojom/OWNERS).

### Spawning a sandbox (broker process)

The main entry point to create a sandbox is `SpawnSandbox` in
[ipc/sandbox.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/ipc/sandbox.cc).
Call this with a `SandboxSetupHooks` subclass.

*  Override methods of `SandboxSetupHooks` to provide custom behaviour for each
   sandbox type, such as instantiating an IPC channel or adding custom flags to
   the sandbox command-line.
*  Each `SandboxSetupHooks` subclass is paired with a `SandboxTargetHooks`
   subclass that knows how to deal with the custom behaviour, eg. connect to
   the IPC channel.
*  The `MojoSandboxSetupHooks` and `MojoSandboxTargetHooks` subclasses contain
   common code to set up a Mojo IPC connection. Override them to provide
   details of the Mojo interface to use with the connection. See below for more
   details.

`SpawnSandbox` will:

1.  Fill in a default sandbox policy, the most restrictive possible.
1.  Add `--sandboxed-process-id=<sandbox type>` to the command-line that will
    be used for the target process.
1.  Call `hooks->UpdateSandboxPolicy`. You can override this to alter the
    policy and command-line.
1.  Spawn another copy of the **current process** to be the target process.
    *  The sandbox library's magic will cause the new process to be sandboxed
       (`GetTargetServices` returns non-null).
    *  The target process will start off suspended.
1.  Call `hooks->TargetSpawned` with handles to the new process and to its main thread.
1.  Resume the target process's main thread. It's now running.
1.  Call `hooks->TargetResumed`.

If any step fails the function calls `hooks->SetupFailed`.

#### Mojo

You will probably want to set up a Mojo IPC channel to your target process. To do this:

1.  Subclass
    [MojoSandboxSetupHooks](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h)
    instead of `SandboxSetupHooks`.
    *  [ZipArchiverSandboxSetupHooks](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/zip_archiver/broker/sandbox_setup.h)
       is a good example.
1.  The hooks class should own a `mojo::Remote<SomeMojoInterface>`. This acts
    like a pointer-to-SomeMojoInterface. Methods called through this "pointer"
    will be marshalled over Mojo IPC to the target process, where
    SomeMojoInterface is implemented.
    *  For the Zip Archiver, this is a `mojo::Remote<mojom::ZipArchiver>`
       defined in
       [zip_archiver.mojom](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/mojom/zip_archiver.mojom).
1.  Override `UpdateSandboxPolicy` to:
    1.  Call `MojoSandboxSetupHooks::SetupSandboxMessagePipe`, which returns a
        handle to the Mojo pipe.
    1.  Create a `mojo::PendingRemote<SomeMojoInterface>` that wraps the pipe
        handle.
    1.  Pass the `PendingRemote` to `mojo::Remote<SomeMojoInterface>::Bind`.
        Now the remote is bound to the Mojo pipe.
1.  You will want a method to take ownership of the remote so that it can be
    saved after the sandbox starts, and passed to whatever code needs to call
    methods on it.
    *  For the Zip Archiver, this is
       `ZipArchiverSandboxSetupHooks::TakeZipArchiverRemote`.

*** note
**Note:** All methods on a `mojo::Remote`, including the destructor, need to be
called from the same sequence. For legacy reasons chrome_cleaner uses an object
called
[MojoTaskRunner](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/ipc/mojo_task_runner.h)
for all remotes.

The easy way to ensure that all methods are called on the same sequence would
be to use
[SequenceBound](https://source.chromium.org/chromium/chromium/src/+/main:base/threading/sequence_bound.h)
but this code predates it, so instead there are lots of explicit calls to
`PostTask` and `OnTaskRunnerDeleter`.
***

### Running a sandbox (target process)

Near the start of main, the app checks if `GetTargetServices` returns non-null.
If so, it's running in the sandbox. (The "Resume the target process's main
thread" step of `SpawnSandbox` has just finished.) At this point the sandboxed
process still has many privileges.

The app then:

1.  Checks the `--sandboxed-process-id` command-line switch to see which type
    of sandbox to start.
1.  Instantiates an override of
    [SandboxTargetHooks](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/ipc/sandbox.h)
    based on the sandbox type.
1.  Calls `RunSandboxTarget` in
    [ipc/sandbox.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/ipc/sandbox.cc),
    which will:
    1.  Initialize the sandbox `TargetService`.
    1.  Call `hooks->TargetStartedWithHighPrivileges`. Override this to do any setup that needs privileges,
        ***note
        **Important:** The sandbox must not handle any **untrusted** data at this step.
        ***
    1.  Call `TargetService::LowerToken` to drop the remaining privileges.
    1.  Call `hooks->TargetDroppedPrivileges`. Override this to do the main processing for the sandbox.

When `RunSandboxTarget` returns, the sandboxed process exits.

*** promo
Most sandboxs enter an endless loop in `hooks->TargetDroppedPrivileges`,
servicing requests sent over IPC from the broker process. The broker will kill
all target processes when it exits.
***

#### Mojo

To connect to a Mojo IPC channel in the target:

1.  Subclass
    [MojoSandboxTargetHooks](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h)
    instead of `SandboxTargetHooks`.
    *  This is paired with a `MojoSandboxSetupHooks` that creates a `mojo::Remote<SomeMojoInterface>`.
    *  [ZipArchiverSandboxTargetHooks](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/zip_archiver/target/sandbox_setup.cc)
       is a good example.
1.  Implement the methods of `SomeMojoInterface`. Traditionally this is done in
    a class called `SomeMojoInterfaceImpl`.
    *  The implementation should own a `mojo::Receiver<SomeMojoInterface>`.
    *  eg. `mojom::ZipArchiver` is implemented in
       [ZipArchiverImpl](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_cleaner/zip_archiver/target/zip_archiver_impl.h),
       and it owns a `mojo::Receiver<mojom::ZipArchier>`.
    *  Note this is in the `target` subdir to enforce that the implementation
       is only used in the target process.
1.  Override `TargetDroppedPrivileges` to:
    1.  Call `MojoSandboxTargetHooks::ExtractSandboxMessagePipe`, which returns
        a handle to the Mojo pipe that was created in
        `MojoSandboxSetupHooks::UpdateSandboxPolicy`.
    1.  Create a `mojo::PendingReceiver<SomeMojoInterface>` that wraps the pipe
        handle.
    1.  Instantiate `SomeMojoInterfaceImpl`, assigning the `PendingReceiver` to the `Receiver`.
        *  This binds the `SomeMojoInterfaceImpl` to the IPC pipe, so all calls
           to methods on the `mojo::Remote<SomeMojoInterface>` in the broker
           process are marshalled across the pipe and invoke the corresponding
           method on the `SomeMojoInterfaceImpl`.
    1.  Call `RunLoop::Run` to loop until the process is killed. Mojo will do the rest.

