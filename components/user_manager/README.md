# UserManager

This directory contains files for managing ChromeOS users. Historically,
the code manages both user and user sessions. There is an on-going effort
to move user session related code into //components/session_manager.

UserManager is the interface for managing ChromeOS users. UserManagerBase
is a base implementation of the interface. There is also a UserManagerInterface
in Chrome code that provides additional UserManager interface that deals with
policy. ChromeUserManager inherits UserManagerBase and UserManagerInterface
it provide a base implementation. Finally, the concrete instance used is
ChromeUserManagerImpl derived from ChromeUserManager.

ChromeUserManagerImpl is created at the PreProfileInit stage and destroyed at
the PostMainMessageLoopRun stage, via
BrowserProcessPlatformPart::InitializeChromeUserManager() and
BrowserProcessPlatformPart::DestroyChromeUserManager.
