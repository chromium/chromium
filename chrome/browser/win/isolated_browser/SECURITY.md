# Windows Isolated Browser Security & Architecture

## 1. Architectural Overview

The Windows Isolated Browser architecture establishes a security boundary around
the primary browser execution by separating execution into three distinct
entities:

```
+-----------------------------------------------------------------+
|                          Stub Process                           |
|  - Non-isolated entry trampoline                                |
|  - Creates Job Object & I/O Completion Port (IOCP)              |
|  - Requests launch from Elevated Service via COM IPC            |
|  - Monitors IOCP until main browser and all children terminate  |
|  - Handles breakaway relaunch on relaunch exit codes            |
+-------------------+-----------------------------+---------------+
                    |                             ^
   (1) Launch IPC   |                             | (3) IOCP monitors exit of
       via COM      v                             |     main & child processes
+---------------------------------------------+   |
|              Elevated Service               |   |
|  - High-privilege Windows service broker    |   |
|  - Spawns Isolated Browser into Job Object  |   |
|  - Validates caller identity & token        |   |
|  - Performs App-Bound Encrypt / Decrypt     |   |
+-------------------+-------------------------+   |
                    |                         ^   |
   (2) Spawns child |                         |   |
       into Job     v                         |   |
+---------------------------------------------+---+---------------+
|                    Isolated Browser Process                     |
|  - Runs with restricted token & `switches::kIsolated`           |
|  - Contained inside Job Object                                  |
|  - Executes standard browser UI and renderer management         |
|  - Calls Elevated Service for App-Bound Encryption via COM (4)  |
|  - Protected from Medium-IL tampering / memory injection        |
+-----------------------------------------------------------------+
```

--------------------------------------------------------------------------------

## 2. Component Roles

### 2.1. The Stub Process

When Chrome is launched,
[`ChromeMainDelegate::RunProcess()`](/chrome/app/chrome_main_delegate.cc)
evaluates whether isolation is enabled via
[`chrome::IsIsolationEnabled()`](isolated_browser_support.h). If active and the
process is not already running with `--isolated`:

1.  **Job Object Creation**: Instantiates a Windows Job Object configured with:
    -   `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`: Guarantees all descendant
        processes are terminated if the job is closed.
    -   `JOB_OBJECT_LIMIT_BREAKAWAY_OK`: Permits authorized breakaway for child
        process management.
2.  **I/O Completion Port (IOCP)**: Associates the Job Object with an IOCP to
    monitor process exit events deterministically, allowing the stub to track
    when the main isolated browser and all child/helper processes have fully
    terminated.
3.  **Child Launch Delegation**: Calls the Elevated Service via COM IPC to
    launch the actual browser process with `--isolated` under the restricted
    token and places it into the configured Job Object.
4.  **Lifecycle Monitoring**: Blocks on
    [`IsolatedBrowserProcess::WaitForExit()`](isolated_browser_support.h#L45)
    until the main isolated browser and all child/helper processes in the job
    object have terminated.

### 2.2. The Isolated Browser Process

The isolated browser process runs with the `--isolated` switch and restricted
security token capabilities:

-   Standard browser features operate within the sandbox / Job Object
    constraints.
-   Any un-isolated operation (e.g., launching un-sandboxed external helpers)
    must explicitly request an un-isolated token via
    [`chrome::GetUnisolatedAccessToken()`](isolated_browser_support.h#L71) and
    supply it to `base::LaunchOptions` via `options.using_token` in
    `base::LaunchProcess()`.

### 2.3. The Elevated Service

The elevated service acts as the privileged launcher and cryptographic broker:

-   Spawns the isolated browser process into the stub's Job Object under
    restricted token policies upon request.
-   Validates and filters command-line switches passed by the stub to ensure
    dangerous or security-sensitive flags cannot be used to spawn the isolated
    browser.
-   Verifies calling process tokens and execution parameters.
-   Encrypts and decrypts sensitive data using App-Bound Encryption, enforcing
    that secrets encrypted by an isolated browser cannot be decrypted by a
    non-isolated browser.

--------------------------------------------------------------------------------

## 3. Command-Line Handling & Relaunch Mechanics

### 3.1. Relaunch Exit Code Signaling

When the isolated browser process requires a restart (e.g., `chrome://restart`,
update relaunch, or flag modifications), it returns a relaunch exit code to the
stub.

### 3.2. Stub Breakaway Spawn

When the isolated browser process terminates:

1.  The waiting **Stub #1** receives a relaunch exit code.
2.  **Stub #1** filters startup flags:
    -   Removes autostart/startup-specific flags.
    -   Appends `--restart`.
3.  **Stub #1** spawns **Stub #2** with `options.force_breakaway_from_job_ =
    true` using `base::LaunchProcess()`. This breaks **Stub #2** out of Job
    Object #1.
4.  **Stub #1** returns `content::RESULT_CODE_NORMAL_EXIT` (`0`) to the
    operating system.
5.  **Stub #2** repeats the initialization flow, creating a clean Job Object #2
    and spawning Isolated Browser #2.

### 3.3. App-Compat & Custom User Data Directories

When Chrome is launched with `--user-data-dir` (such as in automated test
environments, developer profiles, or specialized app-compat scenarios), process
isolation is automatically disabled. Custom user data directories run
un-isolated to ensure maximum compatibility for testing and development
workflows where isolating cryptographic state is not required or feasible.

--------------------------------------------------------------------------------

## 4. Security Model & Threat Boundaries

### 4.1. Command-Line Filtering & Parameter Validation

The Elevated Service enforces strict filtering and validation on all
command-line switches supplied by the stub before spawning the isolated process:

-   Dangerous switches (e.g., `--no-sandbox`, `--remote-debugging-port`,
    `--disable-web-security`, `--single-process`) are blocked or stripped to
    prevent an untrusted caller or compromised stub from undermining sandbox
    isolation or exposing debugging interfaces.
-   Only validated, safe parameters are permitted when launching the isolated
    browser process.
-   TODO: Environment variable filtering is being added to sanitize process
    environment blocks prior to launch.
-   TODO: Defenses against loading untrusted DLLs from user-writable locations
    and COM hijacking are being added. This will be addressed by:
    -   Executing tasks that require loading third-party or untrusted DLLs
        outside the isolation container (e.g., download handlers or specific
        utility processes using un-isolated tokens).
    -   Enforcing Code Integrity Guard (CIG) to block unsigned or untrusted
        binaries from loading into isolated processes.
    -   Disabling `HKEY_CURRENT_USER` (HKCU) registry lookup for COM class
        resolution to mitigate user-level COM hijack attacks.

### 4.2. App-Bound Encryption Integration

App-Bound Encryption incorporates the caller's isolation state
(`IsolationState::kProcessIsolation` vs `IsolationState::kIsolationDisabled`):

-   Data encrypted by an isolated browser instance is tagged with the isolation
    state in the encryption metadata.
-   The elevated decryption service enforces that secrets encrypted by an
    isolated process cannot be decrypted by a non-isolated process.

### 4.3. Authorized Disabling & Dynamic Re-encryption

Users can disable process isolation via a designated UI setting in Chrome:

-   **Dynamic Re-encryption**: Disabling isolation through the approved user
    interface invokes
    [`chrome::SetIsolationState()`](isolated_browser_support.h#L85), which
    dynamically re-encrypts all stored secrets (cookies, passwords, tokens)
    using a non-isolation-bound key prior to restarting into un-isolated mode.
-   **Inaccessibility on Unauthorized Transitions**: This explicit in-browser
    gesture is the **only approved mechanism** to transition from isolated to
    un-isolated mode. Any other attempt to switch modes (e.g., launching Chrome
    with `--user-data-dir` pointing to an existing isolated profile, directly
    modifying the `IsolationState` registry value, or bypassing the stub) will
    leave the data encrypted with an isolation-bound key, making all previously
    stored secrets completely inaccessible to the un-isolated browser.

### 4.4. Threat Model Guarantees

-   **Confidentiality of Pre-Compromise Secrets**: Cookies, tokens, and
    credentials encrypted while running under isolation remain protected against
    exfiltration and offline decryption by non-isolated medium-IL desktop
    processes.
-   **Downgrade Defense**: If an attacker attempts to launch Chrome without
    isolation against a user data directory previously used in isolated mode,
    the non-isolated browser cannot decrypt existing isolated secrets.
-   **Memory Injection & Tampering Defense against Medium-IL**: Standard Medium
    Integrity Level (Medium-IL) desktop processes cannot open full-access
    handles (`PROCESS_VM_WRITE`, `PROCESS_VM_OPERATION`,
    `PROCESS_CREATE_THREAD`) to the isolated browser process due to its
    restricted token and security descriptor constraints. This protects active
    browser memory and decrypted secrets from live tampering or scraping by
    untrusted Medium-IL user processes.

### 4.5. Threat Model Limitations & Out-of-Scope Scenarios

-   **Pre-Generation Tampering & Post-Compromise Modification**: The
    architecture protects existing data encrypted prior to compromise. If an
    attacker with local execution access tampers with shortcuts/icons (e.g.
    redirecting `chrome.exe` to a malicious binary), installs a keylogger, or
    disables isolation *before* new credentials are created, newly entered data
    will not receive isolation protection. Formally acknowledging this boundary
    reflects the inherent limits of user-space isolation against pre-compromise
    local user tampering.
-   **Administrator / SYSTEM / Kernel Privilege Compromise**: Attacks executed
    with Administrator, SYSTEM, or kernel privileges are out of scope, as
    elevated processes can bypass user-space integrity levels and token access
    checks to inspect memory or modify binaries on disk.
-   **UI Automation & Window Message Attacks (UIPI)**: User Interface Privilege
    Isolation (UIPI) is not enforced against the isolated browser's windows to
    preserve compatibility with accessibility software, screen readers, and
    desktop tooling. Attacks where a malicious Medium-IL application synthesizes
    window messages or leverages UI automation to programmatically open the
    Chrome settings page and toggle the isolation switch to off are excluded
    from the threat model, as such attacks do not scale and represent an
    inherent tradeoff for accessibility compatibility.
