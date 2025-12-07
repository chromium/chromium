# Collaboration Messaging Component

This directory contains the public API for the Collaboration Messaging component.

## Overview

The Collaboration Messaging component is a central service for managing and
dispatching collaboration-related messages to the UI. It acts as a bridge
between various backend services and the UI, translating backend events into
user-facing messages and notifications.

The service is responsible for:

*   Processing events from different collaboration-related services.
*   Storing and managing the lifecycle of persistent and instant messages.
*   Providing APIs for the UI to query for messages and activity logs.
*   Notifying observers of changes in message states.

This service depends on and provides messages for other services like
`TabGroupSyncService` and `DataSharingService`.

## Key Concepts

The service distinguishes between two main types of messages:

*   **`PersistentMessage`**: Represents an ongoing UI affordance, such as a
    "dirty" state on a tab or a chip on a tab. These messages are managed via
    the `PersistentMessageObserver`.
*   **`InstantMessage`**: Represents a one-off, immediate notification, such as
    a toast when a user joins a collaboration. These messages are handled by the
    `InstantMessageDelegate`.

The service also provides an **Activity Log** feature, which allows the UI to
query for a list of recent activities within a collaboration.