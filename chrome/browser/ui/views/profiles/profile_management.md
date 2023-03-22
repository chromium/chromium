# On Profile Management

Profile Management refers to the various flows around profile creation and
setting up a user's identity in their profile. The profile picker
([`ProfilePickerView`](profile_picker_view.h)) is the main surface for this.

Many steps, states and asynchronous operations are involved in these UIs, and
to pass context around we rely on a lot of callbacks. This is an overview of the
main callbacks used in the profile management flow, highlighted here through the
first run experience flow.

![sequence diagram](first_run_flow_sequence_diagram.png)

Diagram source:

```mermaid
sequenceDiagram
    autonumber

    participant User
    participant Caller as Caller:<br/>BrowserServiceLacros /<br/>StartupBrowserCreator
    participant FRS as FirstRunService
    participant PPV as ProfilePickerView
    participant FC as FirstRunFlowController

    User->>+Caller: Open Chrome
    activate FRS
    Caller->>FRS: OpenIfNeeded()<br/>with a ResumeTaskCallback<br/>aka void(bool success) callback
    deactivate Caller
    note right of FRS: stores the ResumeTaskCallback and sends<br/> a first_run_exited_callback which is bound to <br/>OnFirstRunHasExited having the signature<br/>void(FirstRunExitStatus)
    FRS->>+PPV: ProfilePicker::Show()<br/>with first_run_exited_callback
    PPV->>+FC: Init()
    Note right of FC: FRE displayed,<br/>user advances through the flow.

    alt flow completed
      User->>FC: completes the flow
      FC->>FRS: in PreFinishWithBrowser: run first_run_exited_callback with a<br/>success boolean based on the status
      Note right of FRS: Handles the exit based on <br/>the status that is passed
      FRS->>+Caller: run ResumeTaskCallback<br/>with success=true
      Caller->>+Browser: launch browser
      deactivate Caller
      FC->>Browser: Opens a new browser or gets an<br/>existing one if present, schedules<br/>post_host_cleared_callback with it
      FC->>PPV: Clear()
      deactivate PPV
      deactivate FC
      FC->>Browser: execute post_host_cleared_callback
      note right of Browser: User proceeds<br/>with their session
      deactivate Browser
      activate PPV
    else flow quitted
      User->>PPV: closing the widget starts destructing ProfilePickerView
      PPV->>FRS: destruction runs first_run_exited_callback<br/>via the params' destructor
      deactivate PPV
      FRS->>+Caller: run ResumeTaskCallback<br/>with success=false
      deactivate Caller
    else chrome opened while first run is running
      User->>+Caller: Open Chrome while the first run is still running
      Caller->>FRS: OpenIfNeeded()<br/>with a ResumeTaskCallback<br/>aka void(bool success) callback
      deactivate Caller
      FRS->>+Caller: The first_run_exited_callback<br/>that was passed in the previous call to<br/>OpenIfNeeded() runs ResumeTaskCallback<br/> with success=false
      deactivate Caller
      FRS->>PPV: ProfilePicker::Show()<br/>with first_run_exited_callback
      Note right of PPV: Opens the profile picker<br/> that has the first run already running.
    end

    deactivate FRS
```
