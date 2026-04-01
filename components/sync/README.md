# Chromium Sync Architecture Overview

Chromium Sync is a system designed to synchronize user data (bookmarks, passwords, history, etc.) across multiple devices. It follows a layered architecture to separate high-level service logic, low-level sync protocol management, and individual data type models. This document describes the high-level architecture.

Additional (more specific/detailed) docs are in [docs/website/site/developers/design-documents/sync/](https://source.chromium.org/chromium/chromium/src/+/main:docs/website/site/developers/design-documents/sync).
You can also visit them via
[chromium.org](https://www.chromium.org/developers/design-documents/sync).

## 1. High-Level Architecture

The system is primarily divided into four parts:

### A. Service (`components/sync/service/`)
This is the main public entry point for the rest of Chromium. It runs on the **UI thread**.
*   **`SyncService` / `SyncServiceImpl`**: The central facade. It manages the overall sync lifecycle (initialization, startup, shutdown) and high-level state.
*   **`SyncUserSettings`**: Manages user preferences, such as which data types are enabled and encryption settings.
*   **`DataTypeController`**: Responsible for managing the state of a single data type (e.g., Bookmarks). It handles the transition between states like loading the model, connecting to the engine, and stopping. Data types may implement a subclass of this to customize the behavior.

Some important private/internal classes:
*   **`DataTypeManager`**: Coordinates the starting and stopping of various data types.
*   **`SyncAuthManager`**: Handles Gaia authentication and access tokens.
*   **`SyncServiceCrypto`**: Manages encryption state and interacts with the "Nigori" (sync encryption) subsystem.

### B. Engine (`components/sync/engine/`)
The engine performs the actual communication with the Sync server. It runs on a dedicated **Sync thread**.
*   **`SyncEngine` / `SyncEngineImpl`**: The main interface to the engine. Note that this lives on the **UI thread**; `SyncEngineBackend` is the corresponding Sync-thread class.
*   **`SyncScheduler`**: Orchestrates when sync cycles should run (based on polling, nudges from local changes, or invalidations from the server).
*   **`DataTypeWorker`**: Responsible for the communication with the sync server, including batching, encryption, etc. Instantiated per data type.
*   **`Syncer`**: The core logic that executes sync cycles (downloading updates via `GetUpdates` and uploading changes via `Commit`).

### C. Model (`components/sync/model/`)
This layer provides the bridge between sync and the individual features / data types (e.g., the Bookmark model). It includes the main APIs that individual features must integrate with in order to Sync (e.g. main data flow). It runs on the **model sequence** (often a backend/database sequence, but it can also be the UI thread in some cases).
*   **`DataTypeSyncBridge`**: An abstract interface that data type owners must implement. It handles merging local and remote data, and receiving and applying remote updates. It is responsible for storing its own sync metadata.
*   **`SyncableService`**: A **deprecated** predecessor to `DataTypeSyncBridge`. Several data types still use it, but it must not be used for any new data types. Unlike `DataTypeSyncBridge`, it does not manage its own metadata directly; instead, it is wrapped in a `SyncableServiceBasedBridge` which uses `DataTypeStore` (LevelDB) for persistence. Historically, this metadata was managed by a central SQLite database called the "Directory", which has since been removed.
*   **`DataTypeLocalChangeProcessor`**: Used by the bridge to report local changes to sync.
*   **`DataTypeStore`**: Offers a simple storage system (LevelDB-based) for features that don't have or need a specific storage system.

### D. Protocol (`components/sync/protocol/`)
*   Contains the **Protocol Buffer (`.proto`)** definitions used for communication between the client and the server. `sync.proto` is the main entry point, with various `*_specifics.proto` files defining the schema for each data type.

---

## 2. Key Data Concepts

*   **Entities**: The basic unit of sync data (e.g., a single bookmark).
*   **Specifics**: The type-specific payload of an entity (e.g., the URL and title of a bookmark).
*   **Metadata**: Information sync needs to track the state of an entity (versions, timestamps, client tags).
*   **Client Tag**: A unique, stable identifier for an entity across all devices.
*   **Nigori**: The protocol used for encryption of sync data.

---

## 3. Core Workflows

### Startup Sequence
1.  `SyncServiceImpl::Initialize()` is called during browser startup.
2.  `SyncAuthManager` fetches an OAuth2 access token.
3.  `SyncServiceImpl` creates the `SyncEngine`.
4.  `DataTypeManager` begins a "configuration" cycle, determining which types should start.
5.  Each `DataTypeController` connects its `DataTypeSyncBridge` (model) to a `DataTypeWorker` (engine).

### Local Change (Commit)
1.  The user modifies data (e.g., saves a password).
2.  The `DataTypeSyncBridge` calls `processor()->Put()`.
3.  The `DataTypeWorker` is notified and "nudges" the `SyncScheduler`.
4.  The `Syncer` performs a `Commit` request, sending the new data to the server.

### Remote Change (Update)
1.  The server sends an invalidation (via FCM) or a poll interval elapses.
2.  The `Syncer` performs a `GetUpdates` request.
3.  New data is received by the `DataTypeWorker` and forwarded to the `DataTypeSyncBridge`.
4.  The bridge merges the remote changes into the local data model via `ApplyIncrementalSyncChanges()`.

---

## 4. Sync Modes

*   **Sync-the-feature**: The classic mode where the user opts in to "Sync everything". It merges local and account data.
*   **Sync-the-transport**: The modern mode where sync runs automatically upon sign-in for certain types, keeping local and account data strictly separate.

---

## 5. Threading Model

Chromium Sync operates across multiple threads to ensure the UI remains responsive and to isolate complex sync logic.

### A. UI Thread
*   **Components**: `SyncService`, `DataTypeManager`, `SyncAuthManager`, `DataTypeController`, `SyncEngine`.
*   **Role**: Handles interaction with the rest of the browser, manages high-level state, and orchestrates the startup/shutdown of data types.

### B. Sync Thread (Sequenced Task Runner)
*   **Components**: `SyncEngineBackend`, `Syncer`, `SyncScheduler`, `DataTypeWorker`.
*   **Role**: Performs all network communication, handles the sync protocol, and manages the sync cycle.
*   **Implementation**: This is typically a `SequencedTaskRunner` created via the `base::ThreadPool` (managed by `SyncEngineFactoryImpl`).

### C. Model Thread(s)
*   **Components**: `DataTypeSyncBridge`, `DataTypeLocalChangeProcessor`, and the actual data models (e.g., `BookmarkModel`).
*   **Role**: Processes local changes and applies remote updates to the local data storage.
*   **Implementation**: The thread depends on the specific data type. Some types (like Bookmarks) run on the UI thread, while others (like Passwords) run on a dedicated background thread.
*   **Communication**: The `ProxyDataTypeControllerDelegate` is used to hop from the UI thread to the model thread when they are different.

### Thread Communication Map
*   **UI -> Sync**: `SyncEngineImpl` (on UI thread) posts tasks to `SyncEngineBackend` (on Sync thread).
*   **Sync -> UI**: `SyncEngineBackend` uses a `WeakPtr` to `SyncEngineImpl` to post notifications (like `OnEngineInitialized`) back to the UI thread.
*   **Model -> Sync**: `DataTypeLocalChangeProcessor` (on Model thread) communicates with `DataTypeWorker` (on Sync thread).
*   **Sync -> Model**: `DataTypeWorker` posts tasks to `DataTypeProcessor` (which lives on the Model thread).
