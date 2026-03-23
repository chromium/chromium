# Account Settings Component

This component manages the data models and synchronization logic for user account settings. It serves as a clean, decoupled layer handling user-specific settings synced across devices.

## High Level Structure
The component primarily manages:
- **AccountSettingService**: Interface for reading and writing account settings.
- **AccountSettingSyncBridge**: Sync integration for user settings.
- **AccountSettingSyncUtil**: Utility helpers for sync model operations.
