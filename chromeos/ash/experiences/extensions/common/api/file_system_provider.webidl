// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[instanceOf=Date]
typedef object Date;

// Error codes used by providing extensions in response to requests as well
// as in case of errors when calling methods of the API. For success,
// <code>"OK"</code> must be used.
enum ProviderError {
  "OK",
  "FAILED",
  "IN_USE",
  "EXISTS",
  "NOT_FOUND",
  "ACCESS_DENIED",
  "TOO_MANY_OPENED",
  "NO_MEMORY",
  "NO_SPACE",
  "NOT_A_DIRECTORY",
  "INVALID_OPERATION",
  "SECURITY",
  "ABORT",
  "NOT_A_FILE",
  "NOT_EMPTY",
  "INVALID_URL",
  "IO"
};

// Mode of opening a file. Used by $(ref:onOpenFileRequested).
enum OpenFileMode {
  "READ",
  "WRITE"
};

// Type of a change detected on the observed directory.
enum ChangeType {
  "CHANGED",
  "DELETED"
};

// List of common actions. <code>"SHARE"</code> is for sharing files with
// others. <code>"SAVE_FOR_OFFLINE"</code> for pinning (saving for offline
// access). <code>"OFFLINE_NOT_NECESSARY"</code> for notifying that the file
// doesn't need to be stored for offline access anymore.
// Used by $(ref:onGetActionsRequested) and $(ref:onExecuteActionRequested).
enum CommonActionId {
  "SAVE_FOR_OFFLINE",
  "OFFLINE_NOT_NECESSARY",
  "SHARE"
};

// Cloud storage representation of a file system entry.
dictionary CloudIdentifier {
  // Identifier for the cloud storage provider (e.g. 'drive.google.com').
  required DOMString providerName;

  // The provider's identifier for the given file/directory.
  required DOMString id;
};

// Information relating to files that are served by a cloud file system.
dictionary CloudFileInfo {
  // A tag that represents the version of the file.
  DOMString versionTag;
};

// Represents metadata of a file or a directory.
dictionary EntryMetadata {
  // True if it is a directory. Must be provided if requested in
  // <code>options</code>.
  boolean isDirectory;

  // Name of this entry (not full path name). Must not contain '/'. For root
  // it must be empty. Must be provided if requested in <code>options</code>.
  DOMString name;

  // File size in bytes. Must be provided if requested in
  // <code>options</code>.
  double size;

  // The last modified time of this entry. Must be provided if requested in
  // <code>options</code>.
  Date modificationTime;

  // Mime type for the entry. Always optional, but should be provided if
  // requested in <code>options</code>.
  DOMString mimeType;

  // Thumbnail image as a data URI in either PNG, JPEG or WEBP format, at most
  // 32 KB in size. Optional, but can be provided only when explicitly
  // requested by the $(ref:onGetMetadataRequested) event.
  DOMString thumbnail;

  // Cloud storage representation of this entry. Must be provided if requested
  // in <code>options</code> and the file is backed by cloud storage. For
  // local files not backed by cloud storage, it should be undefined when
  // requested.
  CloudIdentifier cloudIdentifier;

  // Information that identifies a specific file in the underlying cloud file
  // system. Must be provided if requested in <code>options</code> and the
  // file is backed by cloud storage.
  CloudFileInfo cloudFileInfo;
};

// Represents a watcher.
dictionary Watcher {
  // The path of the entry being observed.
  required DOMString entryPath;

  // Whether watching should include all child entries recursively. It can be
  // true for directories only.
  required boolean recursive;

  // Tag used by the last notification for the watcher.
  DOMString lastTag;
};

// Represents an opened file.
dictionary OpenedFile {
  // A request ID to be be used by consecutive read/write and close requests.
  required long openRequestId;

  // The path of the opened file.
  required DOMString filePath;

  // Whether the file was opened for reading or writing.
  required OpenFileMode mode;
};

// Represents a mounted file system.
dictionary FileSystemInfo {
  // The identifier of the file system.
  required DOMString fileSystemId;

  // A human-readable name for the file system.
  required DOMString displayName;

  // Whether the file system supports operations which may change contents
  // of the file system (such as creating, deleting or writing to files).
  required boolean writable;

  // The maximum number of files that can be opened at once. If 0, then not
  // limited.
  required long openedFilesLimit;

  // List of currently opened files.
  required sequence<OpenedFile> openedFiles;

  // Whether the file system supports the <code>tag</code> field for observing
  // directories.
  boolean supportsNotifyTag;

  // List of watchers.
  required sequence<Watcher> watchers;
};

// Options for the $(ref:mount) method.
dictionary MountOptions {
  // The string indentifier of the file system. Must be unique per each
  // extension.
  required DOMString fileSystemId;

  // A human-readable name for the file system.
  required DOMString displayName;

  // Whether the file system supports operations which may change contents
  // of the file system (such as creating, deleting or writing to files).
  boolean writable;

  // The maximum number of files that can be opened at once. If not specified,
  // or 0, then not limited.
  long openedFilesLimit;

  // Whether the file system supports the <code>tag</code> field for observed
  // directories.
  boolean supportsNotifyTag;

  // Whether the framework should resume the file system at the next sign-in
  // session. True by default.
  boolean persistent;
};

// Options for the $(ref:unmount) method.
dictionary UnmountOptions {
  // The identifier of the file system to be unmounted.
  required DOMString fileSystemId;
};

// Options for the $(ref:onUnmountRequested) event.
dictionary UnmountRequestedOptions {
  // The identifier of the file system to be unmounted.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;
};

// Options for the $(ref:onGetMetadataRequested) event.
dictionary GetMetadataRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The path of the entry to fetch metadata about.
  required DOMString entryPath;

  // Set to <code>true</code> if <code>is_directory</code> value is requested.
  required boolean isDirectory;

  // Set to <code>true</code> if <code>name</code> value is requested.
  required boolean name;

  // Set to <code>true</code> if <code>size</code> value is requested.
  required boolean size;

  // Set to <code>true</code> if <code>modificationTime</code> value is
  // requested.
  required boolean modificationTime;

  // Set to <code>true</code> if <code>mimeType</code> value is requested.
  required boolean mimeType;

  // Set to <code>true</code> if <code>thumbnail</code> value is requested.
  required boolean thumbnail;

  // Set to <code>true</code> if <code>cloudIdentifier</code> value is
  // requested.
  required boolean cloudIdentifier;

  // Set to <code>true</code> if <code>cloudFileInfo</code> value is
  // requested.
  required boolean cloudFileInfo;
};

// Options for the $(ref:onGetActionsRequested) event.
dictionary GetActionsRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // List of paths of entries for the list of actions.
  required sequence<DOMString> entryPaths;
};

// Options for the $(ref:onReadDirectoryRequested) event.
dictionary ReadDirectoryRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The path of the directory which contents are requested.
  required DOMString directoryPath;

  // Set to <code>true</code> if <code>is_directory</code> value is requested.
  required boolean isDirectory;

  // Set to <code>true</code> if <code>name</code> value is requested.
  required boolean name;

  // Set to <code>true</code> if <code>size</code> value is requested.
  required boolean size;

  // Set to <code>true</code> if <code>modificationTime</code> value is
  // requested.
  required boolean modificationTime;

  // Set to <code>true</code> if <code>mimeType</code> value is requested.
  required boolean mimeType;

  // Set to <code>true</code> if <code>thumbnail</code> value is requested.
  required boolean thumbnail;
};

// Options for the $(ref:onOpenFileRequested) event.
dictionary OpenFileRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // A request ID which will be used by consecutive read/write and close
  // requests.
  required long requestId;

  // The path of the file to be opened.
  required DOMString filePath;

  // Whether the file will be used for reading or writing.
  required OpenFileMode mode;
};

// Options for the $(ref:onCloseFileRequested) event.
dictionary CloseFileRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // A request ID used to open the file.
  required long openRequestId;
};

// Options for the $(ref:onReadFileRequested) event.
dictionary ReadFileRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // A request ID used to open the file.
  required long openRequestId;

  // Position in the file (in bytes) to start reading from.
  required double offset;

  // Number of bytes to be returned.
  required double length;
};

// Options for the $(ref:onCreateDirectoryRequested) event.
dictionary CreateDirectoryRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The path of the directory to be created.
  required DOMString directoryPath;

  // Whether the operation is recursive (for directories only).
  required boolean recursive;
};

// Options for the $(ref:onDeleteEntryRequested) event.
dictionary DeleteEntryRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The path of the entry to be deleted.
  required DOMString entryPath;

  // Whether the operation is recursive (for directories only).
  required boolean recursive;
};

// Options for the $(ref:onCreateFileRequested) event.
dictionary CreateFileRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The path of the file to be created.
  required DOMString filePath;
};

// Options for the $(ref:onCopyEntryRequested) event.
dictionary CopyEntryRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The source path of the entry to be copied.
  required DOMString sourcePath;

  // The destination path for the copy operation.
  required DOMString targetPath;
};

// Options for the $(ref:onMoveEntryRequested) event.
dictionary MoveEntryRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The source path of the entry to be moved into a new place.
  required DOMString sourcePath;

  // The destination path for the copy operation.
  required DOMString targetPath;
};

// Options for the $(ref:onTruncateRequested) event.
dictionary TruncateRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The path of the file to be truncated.
  required DOMString filePath;

  // Number of bytes to be retained after the operation completes.
  required double length;
};

// Options for the $(ref:onWriteFileRequested) event.
dictionary WriteFileRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // A request ID used to open the file.
  required long openRequestId;

  // Position in the file (in bytes) to start writing the bytes from.
  required double offset;

  // Buffer of bytes to be written to the file.
  required ArrayBuffer data;
};

// Options for the $(ref:onAbortRequested) event.
dictionary AbortRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // An ID of the request to be aborted.
  required long operationRequestId;
};

// Options for the $(ref:onAddWatcherRequested) event.
dictionary AddWatcherRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The path of the entry to be observed.
  required DOMString entryPath;

  // Whether observing should include all child entries recursively. It can be
  // true for directories only.
  required boolean recursive;
};

// Options for the $(ref:onRemoveWatcherRequested) event.
dictionary RemoveWatcherRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The path of the watched entry.
  required DOMString entryPath;

  // Mode of the watcher.
  required boolean recursive;
};

// Information about an action for an entry.
dictionary Action {
  // The identifier of the action. Any string or $(ref:CommonActionId) for
  // common actions.
  required DOMString id;

  // The title of the action. It may be ignored for common actions.
  DOMString title;
};

// Options for the $(ref:onExecuteActionRequested) event.
dictionary ExecuteActionRequestedOptions {
  // The identifier of the file system related to this operation.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;

  // The set of paths of the entries to be used for the action.
  required sequence<DOMString> entryPaths;

  // The identifier of the action to be executed.
  required DOMString actionId;
};

// Information about a change happened to an entry within the observed
// directory (including the entry itself).
dictionary Change {
  // The path of the changed entry.
  required DOMString entryPath;

  // The type of the change which happened to the entry.
  required ChangeType changeType;

  // Information relating to the file if backed by a cloud file system.
  CloudFileInfo cloudFileInfo;
};

// Options for the $(ref:notify) method.
dictionary NotifyOptions {
  // The identifier of the file system related to this change.
  required DOMString fileSystemId;

  // The path of the observed entry.
  required DOMString observedPath;

  // Mode of the observed entry.
  required boolean recursive;

  // The type of the change which happened to the observed entry. If it is
  // DELETED, then the observed entry will be automatically removed from the
  // list of observed entries.
  required ChangeType changeType;

  // List of changes to entries within the observed directory (including the
  // entry itself)
  sequence<Change> changes;

  // Tag for the notification. Required if the file system was mounted with
  // the <code>supportsNotifyTag</code> option. Note, that this flag is
  // necessary to provide notifications about changes which changed even
  // when the system was shutdown.
  DOMString tag;
};

// Options for the $(ref:onConfigureRequested) event.
dictionary ConfigureRequestedOptions {
  // The identifier of the file system to be configured.
  required DOMString fileSystemId;

  // The unique identifier of this request.
  required long requestId;
};

// Callback to be called by the providing extension in case of a success.
[nocompile] callback ProviderSuccessCallback = undefined();

// Callback to be called by the providing extension in case of an error.
// Any error code is allowed except <code>OK</code>.
[nocompile] callback ProviderErrorCallback = undefined(ProviderError error);

// Success callback for the $(ref:onGetMetadataRequested) event.
[nocompile] callback MetadataCallback = undefined(EntryMetadata metadata);

// Success callback for the $(ref:onGetActionsRequested) event.
[nocompile] callback ActionsCallback = undefined(sequence<Action> actions);

// Success callback for the $(ref:onReadDirectoryRequested) event. If more
// entries will be returned, then <code>hasMore</code> must be true, and it
// has to be called again with additional entries. If no more entries are
// available, then <code>hasMore</code> must be set to false.
[nocompile] callback EntriesCallback = undefined(
    sequence<EntryMetadata> entries, boolean hasMore);

// Success callback for the $(ref:onReadFileRequested) event. If more
// data will be returned, then <code>hasMore</code> must be true, and it
// has to be called again with additional entries. If no more data is
// available, then <code>hasMore</code> must be set to false.
[nocompile] callback FileDataCallback = undefined(
    ArrayBuffer data, boolean hasMore);

// Success callback for the $(ref:onOpenFileRequested) event.
[nocompile] callback OpenFileSuccessCallback = undefined(
    optional EntryMetadata metadata);

callback OnUnmountRequestedListener = undefined(
    UnmountRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnUnmountRequestedEvent : ExtensionEvent {
  static undefined addListener(OnUnmountRequestedListener listener);
  static undefined removeListener(OnUnmountRequestedListener listener);
  static boolean hasListener(OnUnmountRequestedListener listener);
};

callback OnGetMetadataRequestedListener = undefined(
    GetMetadataRequestedOptions options,
    MetadataCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnGetMetadataRequestedEvent : ExtensionEvent {
  static undefined addListener(OnGetMetadataRequestedListener listener);
  static undefined removeListener(OnGetMetadataRequestedListener listener);
  static boolean hasListener(OnGetMetadataRequestedListener listener);
};

callback OnGetActionsRequestedListener = undefined(
    GetActionsRequestedOptions options,
    ActionsCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnGetActionsRequestedEvent : ExtensionEvent {
  static undefined addListener(OnGetActionsRequestedListener listener);
  static undefined removeListener(OnGetActionsRequestedListener listener);
  static boolean hasListener(OnGetActionsRequestedListener listener);
};

callback OnReadDirectoryRequestedListener = undefined(
    ReadDirectoryRequestedOptions options,
    EntriesCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnReadDirectoryRequestedEvent : ExtensionEvent {
  static undefined addListener(OnReadDirectoryRequestedListener listener);
  static undefined removeListener(OnReadDirectoryRequestedListener listener);
  static boolean hasListener(OnReadDirectoryRequestedListener listener);
};

callback OnOpenFileRequestedListener = undefined(
    OpenFileRequestedOptions options,
    OpenFileSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnOpenFileRequestedEvent : ExtensionEvent {
  static undefined addListener(OnOpenFileRequestedListener listener);
  static undefined removeListener(OnOpenFileRequestedListener listener);
  static boolean hasListener(OnOpenFileRequestedListener listener);
};

callback OnCloseFileRequestedListener = undefined(
    CloseFileRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnCloseFileRequestedEvent : ExtensionEvent {
  static undefined addListener(OnCloseFileRequestedListener listener);
  static undefined removeListener(OnCloseFileRequestedListener listener);
  static boolean hasListener(OnCloseFileRequestedListener listener);
};

callback OnReadFileRequestedListener = undefined(
    ReadFileRequestedOptions options,
    FileDataCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnReadFileRequestedEvent : ExtensionEvent {
  static undefined addListener(OnReadFileRequestedListener listener);
  static undefined removeListener(OnReadFileRequestedListener listener);
  static boolean hasListener(OnReadFileRequestedListener listener);
};

callback OnCreateDirectoryRequestedListener = undefined(
    CreateDirectoryRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnCreateDirectoryRequestedEvent : ExtensionEvent {
  static undefined addListener(OnCreateDirectoryRequestedListener listener);
  static undefined removeListener(OnCreateDirectoryRequestedListener listener);
  static boolean hasListener(OnCreateDirectoryRequestedListener listener);
};

callback OnDeleteEntryRequestedListener = undefined(
    DeleteEntryRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnDeleteEntryRequestedEvent : ExtensionEvent {
  static undefined addListener(OnDeleteEntryRequestedListener listener);
  static undefined removeListener(OnDeleteEntryRequestedListener listener);
  static boolean hasListener(OnDeleteEntryRequestedListener listener);
};

callback OnCreateFileRequestedListener = undefined(
    CreateFileRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnCreateFileRequestedEvent : ExtensionEvent {
  static undefined addListener(OnCreateFileRequestedListener listener);
  static undefined removeListener(OnCreateFileRequestedListener listener);
  static boolean hasListener(OnCreateFileRequestedListener listener);
};

callback OnCopyEntryRequestedListener = undefined(
    CopyEntryRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnCopyEntryRequestedEvent : ExtensionEvent {
  static undefined addListener(OnCopyEntryRequestedListener listener);
  static undefined removeListener(OnCopyEntryRequestedListener listener);
  static boolean hasListener(OnCopyEntryRequestedListener listener);
};

callback OnMoveEntryRequestedListener = undefined(
    MoveEntryRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnMoveEntryRequestedEvent : ExtensionEvent {
  static undefined addListener(OnMoveEntryRequestedListener listener);
  static undefined removeListener(OnMoveEntryRequestedListener listener);
  static boolean hasListener(OnMoveEntryRequestedListener listener);
};

callback OnTruncateRequestedListener = undefined(
    TruncateRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnTruncateRequestedEvent : ExtensionEvent {
  static undefined addListener(OnTruncateRequestedListener listener);
  static undefined removeListener(OnTruncateRequestedListener listener);
  static boolean hasListener(OnTruncateRequestedListener listener);
};

callback OnWriteFileRequestedListener = undefined(
    WriteFileRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnWriteFileRequestedEvent : ExtensionEvent {
  static undefined addListener(OnWriteFileRequestedListener listener);
  static undefined removeListener(OnWriteFileRequestedListener listener);
  static boolean hasListener(OnWriteFileRequestedListener listener);
};

callback OnAbortRequestedListener = undefined(
    AbortRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnAbortRequestedEvent : ExtensionEvent {
  static undefined addListener(OnAbortRequestedListener listener);
  static undefined removeListener(OnAbortRequestedListener listener);
  static boolean hasListener(OnAbortRequestedListener listener);
};

callback OnConfigureRequestedListener = undefined(
    ConfigureRequestedOptions options,
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnConfigureRequestedEvent : ExtensionEvent {
  static undefined addListener(OnConfigureRequestedListener listener);
  static undefined removeListener(OnConfigureRequestedListener listener);
  static boolean hasListener(OnConfigureRequestedListener listener);
};

callback OnMountRequestedListener = undefined(
    ProviderSuccessCallback successCallback,
    ProviderErrorCallback errorCallback);

interface OnMountRequestedEvent : ExtensionEvent {
  static undefined addListener(OnMountRequestedListener listener);
  static undefined removeListener(OnMountRequestedListener listener);
  static boolean hasListener(OnMountRequestedListener listener);
};

callback OnAddWatcherRequestedListener = undefined(
  AddWatcherRequestedOptions options,
  ProviderSuccessCallback successCallback,
  ProviderErrorCallback errorCallback);

interface OnAddWatcherRequestedEvent : ExtensionEvent {
  static undefined addListener(OnAddWatcherRequestedListener listener);
  static undefined removeListener(OnAddWatcherRequestedListener listener);
  static boolean hasListener(OnAddWatcherRequestedListener listener);
};

callback OnRemoveWatcherRequestedListener = undefined(
  RemoveWatcherRequestedOptions options,
  ProviderSuccessCallback successCallback,
  ProviderErrorCallback errorCallback);

interface OnRemoveWatcherRequestedEvent : ExtensionEvent {
  static undefined addListener(OnRemoveWatcherRequestedListener listener);
  static undefined removeListener(OnRemoveWatcherRequestedListener listener);
  static boolean hasListener(OnRemoveWatcherRequestedListener listener);
};

callback OnExecuteActionRequestedListener = undefined(
  ExecuteActionRequestedOptions options,
  ProviderSuccessCallback successCallback,
  ProviderErrorCallback errorCallback);

interface OnExecuteActionRequestedEvent : ExtensionEvent {
  static undefined addListener(OnExecuteActionRequestedListener listener);
  static undefined removeListener(OnExecuteActionRequestedListener listener);
  static boolean hasListener(OnExecuteActionRequestedListener listener);
};

// Use the <code>chrome.fileSystemProvider</code> API to create file systems,
// that can be accessible from the file manager on Chrome OS.
[implemented_in="chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"]
interface FileSystemProvider {
  // Mounts a file system with the given <code>fileSystemId</code> and
  // <code>displayName</code>. <code>displayName</code> will be shown in the
  // left panel of the Files app. <code>displayName</code> can contain any
  // characters including '/', but cannot be an empty string.
  // <code>displayName</code> must be descriptive but doesn't have to be
  // unique. The <code>fileSystemId</code> must not be an empty string.
  //
  // Depending on the type of the file system being mounted, the
  // <code>source</code> option must be set appropriately.
  //
  // In case of an error, $(ref:runtime.lastError) will be set with a
  // corresponding error code.
  // |Returns|: A generic result callback to indicate success or failure.
  static Promise<undefined> mount(MountOptions options);

  // Unmounts a file system with the given <code>fileSystemId</code>. It
  // must be called after $(ref:onUnmountRequested) is invoked. Also,
  // the providing extension can decide to perform unmounting if not requested
  // (eg. in case of lost connection, or a file error).
  //
  // In case of an error, $(ref:runtime.lastError) will be set with a
  // corresponding error code.
  // |Returns|: A generic result callback to indicate success or failure.
  static Promise<undefined> unmount(UnmountOptions options);

  // Returns all file systems mounted by the extension.
  // |Returns|: Callback to receive the result of $(ref:getAll) function.
  // |PromiseValue|: fileSystems
  static Promise<sequence<FileSystemInfo>> getAll();

  // Returns information about a file system with the passed
  // <code>fileSystemId</code>.
  // |Returns|: Callback to receive the result of $(ref:get) function.
  // |PromiseValue|: fileSystem
  static Promise<FileSystemInfo> get(DOMString fileSystemId);

  // Notifies about changes in the watched directory at
  // <code>observedPath</code> in <code>recursive</code> mode. If the file
  // system is mounted with <code>supportsNotifyTag</code>, then
  // <code>tag</code> must be provided, and all changes since the last
  // notification always reported, even if the system was shutdown. The last
  // tag can be obtained with $(ref:getAll).
  //
  // To use, the <code>file_system_provider.notify</code> manifest option
  // must be set to true.
  //
  // Value of <code>tag</code> can be any string which is unique per call,
  // so it's possible to identify the last registered notification. Eg. if
  // the providing extension starts after a reboot, and the last registered
  // notification's tag is equal to "123", then it should call $(ref:notify)
  // for all changes which happened since the change tagged as "123". It
  // cannot be an empty string.
  //
  // Not all providers are able to provide a tag, but if the file system has
  // a changelog, then the tag can be eg. a change number, or a revision
  // number.
  //
  // Note that if a parent directory is removed, then all descendant entries
  // are also removed, and if they are watched, then the API must be notified
  // about the fact. Also, if a directory is renamed, then all descendant
  // entries are in fact removed, as there is no entry under their original
  // paths anymore.
  //
  // In case of an error, $(ref:runtime.lastError) will be set
  // will a corresponding error code.
  // |Returns|: A generic result callback to indicate success or failure.
  static Promise<undefined> notify(NotifyOptions options);

  // Raised when unmounting for the file system with the
  // <code>fileSystemId</code> identifier is requested. In the response, the
  // $(ref:unmount) API method must be called together with
  // <code>successCallback</code>. If unmounting is not possible (eg. due to
  // a pending operation), then <code>errorCallback</code> must be called.
  [maxListeners=1] static attribute OnUnmountRequestedEvent onUnmountRequested;

  // Raised when metadata of a file or a directory at <code>entryPath</code>
  // is requested. The metadata must be returned with the
  // <code>successCallback</code> call. In case of an error,
  // <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnGetMetadataRequestedEvent onGetMetadataRequested;

  // Raised when a list of actions for a set of files or directories at
  // <code>entryPaths</code> is requested. All of the returned actions must
  // be applicable to each entry. If there are no such actions, an empty array
  // should be returned. The actions must be returned with the
  // <code>successCallback</code> call. In case of an error,
  // <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnGetActionsRequestedEvent onGetActionsRequested;

  // Raised when contents of a directory at <code>directoryPath</code> are
  // requested. The results must be returned in chunks by calling the
  // <code>successCallback</code> several times. In case of an error,
  // <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnReadDirectoryRequestedEvent onReadDirectoryRequested;

  // Raised when opening a file at <code>filePath</code> is requested. If the
  // file does not exist, then the operation must fail. Maximum number of
  // files opened at once can be specified with <code>MountOptions</code>.
  [maxListeners=1]
  static attribute OnOpenFileRequestedEvent onOpenFileRequested;

  // Raised when opening a file previously opened with
  // <code>openRequestId</code> is requested to be closed.
  [maxListeners=1]
  static attribute OnCloseFileRequestedEvent onCloseFileRequested;

  // Raised when reading contents of a file opened previously with
  // <code>openRequestId</code> is requested. The results must be returned in
  // chunks by calling <code>successCallback</code> several times. In case of
  // an error, <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnReadFileRequestedEvent onReadFileRequested;

  // Raised when creating a directory is requested. The operation must fail
  // with the EXISTS error if the target directory already exists.
  // If <code>recursive</code> is true, then all of the missing directories
  // on the directory path must be created.
  [maxListeners=1]
  static attribute OnCreateDirectoryRequestedEvent onCreateDirectoryRequested;

  // Raised when deleting an entry is requested. If <code>recursive</code> is
  // true, and the entry is a directory, then all of the entries inside
  // must be recursively deleted as well.
  [maxListeners=1]
  static attribute OnDeleteEntryRequestedEvent onDeleteEntryRequested;

  // Raised when creating a file is requested. If the file already exists,
  // then <code>errorCallback</code> must be called with the
  // <code>"EXISTS"</code> error code.
  [maxListeners=1]
  static attribute OnCreateFileRequestedEvent onCreateFileRequested;

  // Raised when copying an entry (recursively if a directory) is requested.
  // If an error occurs, then <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnCopyEntryRequestedEvent onCopyEntryRequested;

  // Raised when moving an entry (recursively if a directory) is requested.
  // If an error occurs, then <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnMoveEntryRequestedEvent onMoveEntryRequested;

  // Raised when truncating a file to a desired length is requested.
  // If an error occurs, then <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnTruncateRequestedEvent onTruncateRequested;

  // Raised when writing contents to a file opened previously with
  // <code>openRequestId</code> is requested.
  [maxListeners=1]
  static attribute OnWriteFileRequestedEvent onWriteFileRequested;

  // Raised when aborting an operation with <code>operationRequestId</code>
  // is requested. The operation executed with <code>operationRequestId</code>
  // must be immediately stopped and <code>successCallback</code> of this
  // abort request executed. If aborting fails, then
  // <code>errorCallback</code> must be called. Note, that callbacks of the
  // aborted operation must not be called, as they will be ignored. Despite
  // calling <code>errorCallback</code>, the request may be forcibly aborted.
  [maxListeners=1] static attribute OnAbortRequestedEvent onAbortRequested;

  // Raised when showing a configuration dialog for <code>fileSystemId</code>
  // is requested. If it's handled, the
  // <code>file_system_provider.configurable</code> manfiest option must be
  // set to true.
  [maxListeners=1]
  static attribute OnConfigureRequestedEvent onConfigureRequested;

  // Raised when showing a dialog for mounting a new file system is requested.
  // If the extension/app is a file handler, then this event shouldn't be
  // handled. Instead <code>app.runtime.onLaunched</code> should be handled in
  // order to mount new file systems when a file is opened. For multiple
  // mounts, the <code>file_system_provider.multiple_mounts</code> manifest
  // option must be set to true.
  [maxListeners=1] static attribute OnMountRequestedEvent onMountRequested;

  // Raised when setting a new directory watcher is requested. If an error
  // occurs, then <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnAddWatcherRequestedEvent onAddWatcherRequested;

  // Raised when the watcher should be removed. If an error occurs, then
  // <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnRemoveWatcherRequestedEvent onRemoveWatcherRequested;

  // Raised when executing an action for a set of files or directories is\
  // requested. After the action is completed, <code>successCallback</code>
  // must be called. On error, <code>errorCallback</code> must be called.
  [maxListeners=1]
  static attribute OnExecuteActionRequestedEvent onExecuteActionRequested;
};

partial interface Browser {
  static attribute FileSystemProvider fileSystemProvider;
};
