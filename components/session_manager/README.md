# SessionManager

This directory contains files for managing ChromeOS user sessions. It is
a continuing effort to move the user session related code from UserManager
and UserSessionManager to this directory.

It is put under components/ directory so that non-browser code could access
it. However, ash/ code should access user sessions via ash::SessionController
rather than this. SessionController/Client internally calls code here but
it provides a separation of ash and browser. It would be nice to keep it.

The core/ directory contains the SessionManager class. It provides an interface
to create user sessions, add/remove observers, and check user session state.
User session managing methods are not ready for external code to use. They
are still expected to be called from the current user session starting code
path, i.e. UserSessionManager and ChromeSessionManager.

SessionManager::Get() method gets the singleton like instance. The instance
in Chrome browser is ChromeSessionManager derived from SessionManager. This
is needed because SessionManager logic depends on policy code which is still
part of the browser. ChromeSessionManager is in browser/ and can access that.
The ChromeSessionManager instance is created in the PreCreateMainMessageLoop
stage and destroyed in the PostMainMessageLoopRun.
