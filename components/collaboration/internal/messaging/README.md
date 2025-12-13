# Collaboration Messaging Component (Internal)

This directory contains the internal implementation of the Collaboration
Messaging component. For the public API, see
[//components/collaboration/public/messaging/README.md](https://source.chromium.org/chromium/chromium/src/+/main:components/collaboration/public/messaging/README.md).

## Overview

The Collaboration Messaging component is a central service for managing and
dispatching collaboration-related messages to the UI. It acts as a bridge
between various backend services (like `TabGroupSyncService` and
`DataSharingService`) and the UI, translating backend events into user-facing
messages and notifications.

## Architecture

The main components of the architecture are:

*   **`MessagingBackendService`**: The central service that manages and
    dispatches messages. It is a `KeyedService` and is tied to a user's
    profile. The implementation is in `MessagingBackendServiceImpl`.

*   **`TabGroupChangeNotifier`** and **`DataSharingChangeNotifier`**: These
    helper classes observe the `TabGroupSyncService` and
    `DataSharingService` respectively, and provide a simpler, delta-based
    observer API for the `MessagingBackendService`. The implementations
    (`TabGroupChangeNotifierImpl` and `DataSharingChangeNotifierImpl`) are
    responsible for translating the backend service's specific events into a
    common format that the messaging service can understand.

*   **`MessagingBackendStore`**: This component provides an abstraction over a
    database for storing messages. It is responsible for persistence of
    messages and activity logs. The implementation is located in the `storage`
    subdirectory.

*   **`InstantMessageProcessor`**: This component queues, processes, and
    aggregates instant messages. This is useful for preventing message spam
    and for aggregating multiple related events into a single, more concise
    message. The implementation is in `InstantMessageProcessorImpl`.

These components work together to provide a unified messaging service. The
`MessagingBackendServiceImpl` owns the notifiers, the store, and the
processor. It receives events from the notifiers, processes them, and then
either stores them as persistent messages or dispatches them as instant
messages.
