# UserManager

This directory contains files for managing ChromeOS users. Historically,
the code manages both user and user sessions. There is an on-going effort
to move user session related code into //components/session_manager.

UserManager is the interface for managing ChromeOS users. UserManagerImpl
is the production implementation of the interface.

UserManagerImpl is created at the PreProfileInit stage and destroyed at
the PostMainMessageLoopRun stage, via
BrowserProcessPlatformPart::InitializeUserManager() and
BrowserProcessPlatformPart::DestroyUserManager.

# UserDirectoryIntegrityManager

This directory contains the UserDirectoryIntegrityManager class.
This class is responsible for detecting when a user did not completely
complete the onboarding flow and crashed before adding an auth factor.
This can leave the user in an inconsistent state as their home directory
will be encrypted with no auth factors added.

UserDirectoryIntegrityManager writes the user's email to local state
before creating the user home directory and erases it after adding a
successful auth factor. Those operations are initiated from
`MountPerformer::CreateNewUser` and `AuthFactorEditor::OnAddCredentials`
respectively.

[design doc](https://docs.google.com/document/d/1SjVwlckD-cB9zC84RfmHbsflD3g01lKm8m21Dh1sn70/edit?resourcekey=0-SseJsx99IUSckOWDqHh69g#)
