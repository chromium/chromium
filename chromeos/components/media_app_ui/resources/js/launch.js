// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sort order for files in the navigation ring.
 * @enum
 */
const SortOrder = {
  /**
   * Lexicographic (with natural number ordering): advancing goes "down" the
   * alphabet.
   */
  A_FIRST: 1,
  /**
   * Reverse lexicographic (with natural number ordering): advancing goes "up"
   * the alphabet.
   */
  Z_FIRST: 2,
  /** By modified time: pressing "right" goes to older files. */
  NEWEST_FIRST: 3,
};

/**
 * Wrapper around a file handle that allows the privileged context to arbitrate
 * read and write access as well as file navigation. `token` uniquely identifies
 * the file, `file` temporarily holds the object passed over postMessage, and
 * `handle` allows it to be reopened upon navigation. If an error occurred on
 * the last attempt to open `handle`, `lastError` holds the error name.
 * @typedef {{
 *     token: number,
 *     file: ?File,
 *     handle: !FileSystemFileHandle,
 *     lastError: (string|undefined),
 *     inCurrentDirectory: (boolean|undefined),
 * }}
 */
let FileDescriptor;

/**
 * Array of entries available in the current directory.
 *
 * @type {!Array<!FileDescriptor>}
 */
const currentFiles = [];

/**
 * The current sort order.
 * TODO(crbug/414789): Match the file manager order when launched that way.
 * Note currently this is reassigned in tests.
 * @type {!SortOrder}
 */
// eslint-disable-next-line prefer-const
let sortOrder = SortOrder.Z_FIRST;

/**
 * Index into `currentFiles` of the current file.
 *
 * @type {number}
 */
let entryIndex = -1;

/**
 * Keeps track of the current launch (i.e. call to `launchWithDirectory`) .
 * Since file loading can be deferred i.e. we can load the first focused file
 * and start using the app then load other files in `loadOtherRelatedFiles()` we
 * need to make sure `loadOtherRelatedFiles` gets aborted if it is out of date
 * i.e. in interleaved launches.
 *
 * @type {number}
 */
let globalLaunchNumber = -1;

/**
 * Reference to the directory handle that contains the first file in the most
 * recent launch event.
 * @type {?FileSystemDirectoryHandle}
 */
let currentDirectoryHandle = null;

/**
 * Map of file tokens. Persists across new launch requests from the file
 * manager when chrome://media-app has not been closed.
 * @type {!Map<number, !FileSystemFileHandle>}
 */
const tokenMap = new Map();

/**
 * A pipe through which we can send messages to the guest frame.
 * Use an undefined `target` to find the <iframe> automatically.
 * Do not rethrow errors, since handlers installed here are expected to
 * throw exceptions that are handled on the other side of the pipe. And
 * nothing `awaits` async callHandlerForMessageType_(), so they will always
 * be reported as `unhandledrejection` and trigger a crash report.
 */
const guestMessagePipe =
    new MessagePipe('chrome-untrusted://media-app', undefined, false);

/**
 * Promise that resolves once the iframe is ready to receive messages. This is
 * to allow initial file processing to run in parallel with the iframe load.
 * @type {!Promise<undefined>}
 */
const iframeReady = new Promise(resolve => {
  guestMessagePipe.registerHandler(Message.IFRAME_READY, resolve);
});

guestMessagePipe.registerHandler(Message.OPEN_FEEDBACK_DIALOG, () => {
  let response = mediaAppPageHandler.openFeedbackDialog();
  if (response === null) {
    response = {errorMessage: 'Null response received'};
  }
  return response;
});

guestMessagePipe.registerHandler(Message.OVERWRITE_FILE, async (message) => {
  const overwrite = /** @type {!OverwriteFileMessage} */ (message);
  const originalHandle = fileHandleForToken(overwrite.token);
  try {
    await saveBlobToFile(originalHandle, overwrite.blob);
  } catch (/** @type {!DOMException|!Error} */ e) {
    // TODO(b/160843424): Collect UMA.
    console.warn('Showing a picker due to', e);
    return pickFileForFailedOverwrite(originalHandle.name, e.name, overwrite);
  }
});

/**
 * Shows a file picker and redirects a failed OverwriteFileMessage to the chosen
 * file. Updates app state and rebinds file tokens if the write is successful.
 * @param {string} fileName
 * @param {string} errorName
 * @param {!OverwriteFileMessage} overwrite
 * @return {!Promise<!OverwriteViaFilePickerResponse>}
 */
async function pickFileForFailedOverwrite(fileName, errorName, overwrite) {
  const fileHandle = await pickWritableFile(fileName, overwrite.blob.type);
  await saveBlobToFile(fileHandle, overwrite.blob);

  // Success. Replace the old handle.
  tokenMap.set(overwrite.token, fileHandle);
  const entry = currentFiles.find(i => i.token === overwrite.token);
  if (entry) {
    entry.handle = fileHandle;
  }
  return {renamedTo: fileHandle.name, errorName};
}

guestMessagePipe.registerHandler(Message.DELETE_FILE, async (message) => {
  const deleteMsg = /** @type {!DeleteFileMessage} */ (message);
  const {handle, directory} =
      assertFileAndDirectoryMutable(deleteMsg.token, 'Delete');

  if (!(await isHandleInCurrentDirectory(handle))) {
    return {deleteResult: DeleteResult.FILE_MOVED};
  }

  // Get the name from the file reference. Handles file renames.
  const currentFilename = (await handle.getFile()).name;

  await directory.removeEntry(currentFilename);

  // Remove the file that was deleted.
  currentFiles.splice(entryIndex, 1);

  // Attempts to load the file to the right which is at now at
  // `currentFiles[entryIndex]`, where `entryIndex` was previously the index of
  // the deleted file.
  await advance(0);

  return {deleteResult: DeleteResult.SUCCESS};
});

/** Handler to rename the currently focused file. */
guestMessagePipe.registerHandler(Message.RENAME_FILE, async (message) => {
  const renameMsg = /** @type {!RenameFileMessage} */ (message);
  const {handle, directory} =
      assertFileAndDirectoryMutable(renameMsg.token, 'Rename');

  if (await filenameExistsInCurrentDirectory(renameMsg.newFilename)) {
    return {renameResult: RenameResult.FILE_EXISTS};
  }

  const originalFile = await handle.getFile();
  let originalFileIndex =
      currentFiles.findIndex(fd => fd.token === renameMsg.token);

  if (originalFileIndex < 0) {
    return {renameResult: RenameResult.FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY};
  }

  const renamedFileHandle =
      await directory.getFileHandle(renameMsg.newFilename, {create: true});
  // Copy file data over to the new file.
  const writer = await renamedFileHandle.createWritable();
  const sink = /** @type {!WritableStream<*>} */ (writer);
  const source =
      /** @type {{stream: function(): !ReadableStream}} */ (originalFile);
  await source.stream().pipeTo(sink);

  // Remove the old file since the new file has all the data & the new name.
  // Note even though removing an entry that doesn't exist is considered
  // success, we first check `handle` is the same as the handle for the file
  // with that filename in the `currentDirectoryHandle`.
  if (await isHandleInCurrentDirectory(handle)) {
    await directory.removeEntry(originalFile.name);
  }

  // Replace the old file in our internal representation. There is no harm using
  // the old file's token since the old file is removed.
  tokenMap.set(renameMsg.token, renamedFileHandle);
  // Remove the entry for `originalFile` in current files, replace it with a
  // FileDescriptor for the renamed file.

  const renamedFile = await renamedFileHandle.getFile();
  // Ensure the file is still in `currentFiles` after all the above `awaits`. If
  // missing it means either new files have loaded (or tried to), see
  // b/164985809.
  originalFileIndex =
      currentFiles.findIndex(fd => fd.token === renameMsg.token);

  if (originalFileIndex < 0) {
    // Can't navigate to the renamed file so don't add it to `currentFiles`.
    return {renameResult: RenameResult.SUCCESS};
  }

  currentFiles.splice(originalFileIndex, 1, {
    token: renameMsg.token,
    file: renamedFile,
    handle: renamedFileHandle,
    inCurrentDirectory: true
  });

  return {renameResult: RenameResult.SUCCESS};
});

guestMessagePipe.registerHandler(Message.NAVIGATE, async (message) => {
  const navigate = /** @type {!NavigateMessage} */ (message);

  await advance(navigate.direction, navigate.currentFileToken);
});

guestMessagePipe.registerHandler(Message.REQUEST_SAVE_FILE, async (message) => {
  const {suggestedName, mimeType} =
      /** @type {!RequestSaveFileMessage} */ (message);
  const handle = await pickWritableFile(suggestedName, mimeType);
  /** @type {!RequestSaveFileResponse} */
  const response = {
    pickedFileContext: {
      token: generateToken(handle),
      file: assertCast(await handle.getFile()),
      name: handle.name,
      error: '',
      canDelete: false,
      canRename: false,
    }
  };
  return response;
});

guestMessagePipe.registerHandler(Message.SAVE_AS, async (message) => {
  const {blob, oldFileToken, pickedFileToken} =
      /** @type {!SaveAsMessage} */ (message);
  const oldFileDescriptor = currentFiles.find(fd => fd.token === oldFileToken);
  /** @type {!FileDescriptor} */
  const pickedFileDescriptor = {
    // We silently take over the old file's file descriptor by taking its token,
    // note we can be passed an undefined token if the file we are saving was
    // dragged into the media app.
    token: oldFileToken || tokenGenerator.next().value,
    file: null,
    handle: tokenMap.get(pickedFileToken)
  };
  const oldFileIndex = currentFiles.findIndex(fd => fd.token === oldFileToken);
  tokenMap.set(pickedFileDescriptor.token, pickedFileDescriptor.handle);
  // Give the old file a new token, if we couldn't find the old file we assume
  // its been deleted (or pasted/dragged into the media app) and skip this
  // step.
  if (oldFileDescriptor) {
    oldFileDescriptor.token = generateToken(oldFileDescriptor.handle);
  }
  try {
    // Note `pickedFileHandle` could be the same as a `FileSystemFileHandle`
    // that exists in `tokenMap`. Possibly even the `File` currently open. But
    // that's OK. E.g. the next overwrite-file request will just invoke
    // `saveBlobToFile` in the same way. Note there may be no currently writable
    // file (e.g. save from clipboard).
    await saveBlobToFile(pickedFileDescriptor.handle, blob);
  } catch (/** @type {!DOMException} */ e) {
    // If something went wrong revert the token back to its original
    // owner so future file actions function correctly.
    if (oldFileDescriptor && oldFileToken) {
      oldFileDescriptor.token = oldFileToken;
      tokenMap.set(oldFileToken, oldFileDescriptor.handle);
    }
    throw e;
  }

  // Note: oldFileIndex may be `-1` here which causes the new file to be added
  // to the start of the array, this is WAI.
  currentFiles.splice(oldFileIndex + 1, 0, pickedFileDescriptor);
  // Silently update entry index without triggering a reload of the media app.
  entryIndex = oldFileIndex + 1;

  /** @type {!SaveAsResponse} */
  const response = {newFilename: pickedFileDescriptor.handle.name};
  return response;
});

guestMessagePipe.registerHandler(Message.OPEN_FILE, async () => {
  const [handle] = await window.showOpenFilePicker({multiple: false});
  /** @type {!FileDescriptor} */
  const fileDescriptor = {
    token: generateToken(handle),
    file: null,
    handle: handle,
    inCurrentDirectory: false
  };
  currentFiles.splice(entryIndex + 1, 0, fileDescriptor);
  advance(1);
});

/**
 * Shows a file picker to get a writable file.
 * @param {string} suggestedName
 * @param {string} mimeType
 * @return {!Promise<!FileSystemFileHandle>}
 */
function pickWritableFile(suggestedName, mimeType) {
  const extension = '.' + suggestedName.split('.').reverse()[0];
  // TODO(b/161087799): Add a default filename when it's supported by the
  // native file api.
  /** @type {!FilePickerOptions} */
  const options = {
    types: [
      {description: extension, accept: {[mimeType]: [extension]}},
    ],
    excludeAcceptAllOption: true,
  };
  // This may throw an error, but we can handle and recover from it on the
  // unprivileged side.
  return window.showSaveFilePicker(options);
}

/**
 * Generator instance for unguessable tokens.
 * @suppress {reportUnknownTypes} Typing of yield is broken (b/142881197).
 * @type {!Generator<number>}
 */
const tokenGenerator = (function*() {
  // To use the regular number type, tokens must stay below
  // Number.MAX_SAFE_INTEGER (2^53). So stick with ~33 bits. Note we can not
  // request more than 64kBytes from crypto.getRandomValues() at a time.
  const randomBuffer = new Uint32Array(1000);
  while (true) {
    assertCast(crypto).getRandomValues(randomBuffer);
    for (let i = 0; i < randomBuffer.length; ++i) {
      const token = randomBuffer[i];
      // Disallow "0" as a token.
      if (token && !tokenMap.has(token)) {
        yield Number(token);
      }
    }
  }
})();

/**
 * Generate a file token, and persist the mapping to `handle`.
 * @param {!FileSystemFileHandle} handle
 * @return {number}
 */
function generateToken(handle) {
  const token = tokenGenerator.next().value;
  tokenMap.set(token, handle);
  return token;
}

/**
 * Returns the `FileSystemFileHandle` for the given `token`. This is
 * "guaranteed" to succeed: tokens are only generated once a file handle has
 * been successfully opened at least once (and determined to be "related"). The
 * handle doesn't expire, but file system operations may fail later on.
 * One corner case, however, is when the initial file open fails and the token
 * gets replaced by `-1`. File operations all need to fail in that case.
 * @param {number} token
 * @return {!FileSystemFileHandle}
 */
function fileHandleForToken(token) {
  const handle = tokenMap.get(token);
  if (!handle) {
    throw new DOMException(`No handle for token(${token})`, 'NotFoundError');
  }
  return handle;
}

/**
 * Saves the provided blob the provided fileHandle. Assumes the handle is
 * writable.
 * @param {!FileSystemFileHandle} handle
 * @param {!Blob} data
 * @return {!Promise<undefined>}
 */
async function saveBlobToFile(handle, data) {
  const writer = await handle.createWritable();
  await writer.write(data);
  await writer.truncate(data.size);
  await writer.close();
}

/**
 * Warns if a given exception is "uncommon". That is, one that the guest might
 * not provide UX for and should be dumped to console to give additional
 * context.
 * @param {!DOMException} e
 * @param {string} fileName
 */
function warnIfUncommon(e, fileName) {
  if (e.name === 'NotFoundError' || e.name === 'NotAllowedError') {
    return;
  }
  console.warn(`Unexpected ${e.name} on ${fileName}: ${e.message}`);
}

/**
 * If `fd.file` is null, re-opens the file handle in `fd`.
 * @param {!FileDescriptor} fd
 */
async function refreshFile(fd) {
  if (fd.file) {
    return;
  }
  fd.lastError = '';
  try {
    fd.file = (await getFileFromHandle(fd.handle)).file;
  } catch (/** @type {!DOMException} */ e) {
    fd.lastError = e.name;
    // A failure here is only a problem for the "current" file (and that needs
    // to be handled in the unprivileged context), so ignore known errors.
    warnIfUncommon(e, fd.handle.name);
  }
}

/**
 * Loads the current file list into the guest.
 * @return {!Promise<undefined>}
 */
async function sendFilesToGuest() {
  return sendSnapshotToGuest(
      [...currentFiles], globalLaunchNumber);  // Shallow copy.
}

/**
 * Converts a file descriptor from `currentFiles` into a `FileContext` used by
 * the LoadFilesMessage. Closure forgets that some fields may be missing without
 * naming the type explicitly on the signature here.
 * @param {!FileDescriptor} fd
 * @return {!FileContext}
 */
function fileDescriptorToFileContext(fd) {
  // TODO(b/163285796): Properly detect files that can't be renamed/deleted.
  return {
    token: fd.token,
    file: fd.file,
    name: fd.handle.name,
    error: fd.lastError || '',
    canDelete: fd.inCurrentDirectory || false,
    canRename: fd.inCurrentDirectory || false,
  };
}

/**
 * Loads the provided file list into the guest without making any file writable.
 * Note: code paths can defer loads i.e. `launchWithDirectory()` increment
 * `globalLaunchNumber` to ensure their deferred load is still relevant when it
 * finishes processing. Other code paths that call `sendSnapshotToGuest()` don't
 * have to.
 * @param {!Array<!FileDescriptor>} snapshot
 * @param {number} localLaunchNumber
 * @param {boolean=} extraFiles
 * @return {!Promise<undefined>}
 */
async function sendSnapshotToGuest(
    snapshot, localLaunchNumber, extraFiles = false) {
  const focusIndex = entryIndex;

  // On first launch, files are opened to determine navigation candidates. Don't
  // reopen in that case. Otherwise, attempt to reopen the focus file only. In
  // future we might also open "nearby" files for preloading. However, reopening
  // *all* files on every navigation attempt to verify they can still be
  // navigated to adds noticeable lag in large directories.
  if (focusIndex >= 0 && focusIndex < snapshot.length) {
    await refreshFile(snapshot[focusIndex]);
  } else if (snapshot.length !== 0) {
    await refreshFile(snapshot[0]);
  }
  if (localLaunchNumber !== globalLaunchNumber) {
    return;
  }

  /** @type {!LoadFilesMessage} */
  const loadFilesMessage = {
    writableFileIndex: focusIndex,
    // Handle can't be passed through a message pipe.
    files: snapshot.map(fileDescriptorToFileContext)
  };
  // Clear handles to the open files in the privileged context so they are
  // refreshed on a navigation request. The refcount to the File will be alive
  // in the postMessage object until the guest takes its own reference.
  for (const fd of snapshot) {
    fd.file = null;
  }
  await iframeReady;
  if (extraFiles) {
    await guestMessagePipe.sendMessage(
        Message.LOAD_EXTRA_FILES, loadFilesMessage);
  } else {
    await guestMessagePipe.sendMessage(Message.LOAD_FILES, loadFilesMessage);
  }
}

/**
 * Throws an error if the file or directory handles don't exist or the token for
 * the file to be mutated is incorrect.
 * @param {number} editFileToken
 * @param {string} operation
 * @return {{handle: !FileSystemFileHandle, directory:
 *     !FileSystemDirectoryHandle}}
 */
function assertFileAndDirectoryMutable(editFileToken, operation) {
  if (!currentDirectoryHandle) {
    throw new Error(`${operation} failed. File without launch directory.`);
  }

  return {
    handle: fileHandleForToken(editFileToken),
    directory: currentDirectoryHandle
  };
}

/**
 * Returns whether `handle` is in `currentDirectoryHandle`. Prevents mutating a
 * file that doesn't exist.
 * @param {!FileSystemFileHandle} handle
 * @return {!Promise<boolean>}
 */
async function isHandleInCurrentDirectory(handle) {
  // Get the name from the file reference. Handles file renames.
  const currentFilename = (await handle.getFile()).name;
  const fileHandle = await getFileHandleFromCurrentDirectory(currentFilename);
  return fileHandle ? fileHandle.isSameEntry(handle) : false;
}

/**
 * Returns if a`filename` exists in `currentDirectoryHandle`.
 * @param {string} filename
 * @return {!Promise<boolean>}
 */
async function filenameExistsInCurrentDirectory(filename) {
  return (await getFileHandleFromCurrentDirectory(filename, true)) !== null;
}

/**
 * Returns the `FileSystemFileHandle` for `filename` if it exists in the current
 * directory, otherwise null.
 * @param {string} filename
 * @param {boolean=} suppressError
 * @return {!Promise<!FileSystemHandle|null>}
 */
async function getFileHandleFromCurrentDirectory(
    filename, suppressError = false) {
  if (!currentDirectoryHandle) {
    return null;
  }
  try {
    return (
        await currentDirectoryHandle.getFileHandle(filename, {create: false}));
  } catch (/** @type {?Object} */ e) {
    if (!suppressError) {
      console.error(e);
    }
    return null;
  }
}

/**
 * Gets a file from a handle received via the fileHandling API. Only handles
 * expected to be files should be passed to this function. Throws a DOMException
 * if opening the file fails - usually because the handle is stale.
 * @param {?FileSystemHandle} fileSystemHandle
 * @return {!Promise<{file: !File, handle: !FileSystemFileHandle}>}
 */
async function getFileFromHandle(fileSystemHandle) {
  if (!fileSystemHandle || fileSystemHandle.kind !== 'file') {
    // Invent our own exception for this corner case. It might happen if a file
    // is deleted and replaced with a directory with the same name.
    throw new DOMException('Not a file.', 'NotAFile');
  }
  const handle = /** @type {!FileSystemFileHandle} */ (fileSystemHandle);
  const file = await handle.getFile();  // Note: throws DOMException.
  return {file, handle};
}

/**
 * Returns whether `file` is a video or image file.
 * @param {!File} file
 * @return {boolean}
 */
function isVideoOrImage(file) {
  // Check for .mkv explicitly because it is not a web-supported type, but is in
  // common use on ChromeOS.
  return /^(image)|(video)\//.test(file.type) ||
      /\.mkv$/.test(file.name.toLowerCase());
}

/**
 * Returns whether `siblingFile` is related to `focusFile`. That is, whether
 * they should be traversable from one another. Usually this means they share a
 * similar (non-empty) MIME type.
 * @param {!File} focusFile The file selected by the user.
 * @param {!File} siblingFile A file in the same directory as `focusFile`.
 * @return {boolean}
 */
function isFileRelated(focusFile, siblingFile) {
  return focusFile.name === siblingFile.name ||
      (!!focusFile.type && focusFile.type === siblingFile.type) ||
      (isVideoOrImage(focusFile) && isVideoOrImage(siblingFile));
}

/**
 * Enum like return value of `processOtherFilesInDirectory()`.
 * @enum {number}
 */
const ProcessOtherFilesResult = {
  // Newer load in progress, can abort loading these files.
  ABORT: -2,
  // The focusFile is missing, treat this as a normal load.
  FOCUS_FILE_MISSING: -1,
  // The focusFile is present, load these files as extra files.
  FOCUS_FILE_RELEVANT: 0,
};

/**
 * Loads related files the working directory to initialize file iteration
 * according to the type of the opened file. If `globalLaunchNumber` changes
 * (i.e. another launch occurs), this will abort early and not change
 * `currentFiles`.
 * @param {!FileSystemDirectoryHandle} directory
 * @param {?File} focusFile
 * @param {number} localLaunchNumber
 * @return {!Promise<!ProcessOtherFilesResult>}
 */
async function processOtherFilesInDirectory(
    directory, focusFile, localLaunchNumber) {
  if (!focusFile || !focusFile.name) {
    return ProcessOtherFilesResult.ABORT;
  }

  /** @type {!Array<!FileDescriptor>} */
  const relatedFiles = [];
  // TODO(b/158149714): Clear out old tokens as well? Care needs to be taken to
  // ensure any file currently open with unsaved changes can still be saved.
  for await (const /** !FileSystemHandle */ handle of directory.values()) {
    if (localLaunchNumber !== globalLaunchNumber) {
      // Abort, another more up to date launch in progress.
      return ProcessOtherFilesResult.ABORT;
    }

    if (handle.kind !== 'file') {
      continue;
    }
    let entry = null;
    try {
      entry = await getFileFromHandle(handle);
    } catch (/** @type {!DOMException} */ e) {
      // Ignore exceptions thrown trying to open "other" files in the folder,
      // and skip adding that file to `currentFiles`.
      // Note the focusFile is passed in as `File`, so should be openable.
      warnIfUncommon(e, handle.name);
    }

    // Only allow traversal of related file types.
    if (entry && isFileRelated(focusFile, entry.file)) {
      // Note: The focus file will be processed here again but will be skipped
      // over when added to `currentFiles`.
      relatedFiles.push({
        token: generateToken(entry.handle),
        file: entry.file,
        handle: entry.handle,
        inCurrentDirectory: true,
      });
    }
  }

  if (localLaunchNumber !== globalLaunchNumber) {
    return ProcessOtherFilesResult.ABORT;
  }

  // Iteration order is not guaranteed using `directory.entries()`, so we
  // sort it afterwards by modification time to ensure a consistent and logical
  // order. More recent (i.e. higher timestamp) files should appear first. In
  // the case where timestamps are equal, the files will be sorted
  // lexicographically according to their names.
  relatedFiles.sort((a, b) => {
    // Sort null files last if they racily appear.
    if (!a.file && !b.file) {
      return 0;
    } else if (!b.file) {
      return -1;
    } else if (!a.file) {
      return 1;
    }
    if (sortOrder === SortOrder.NEWEST_FIRST) {
      if (a.file.lastModified === b.file.lastModified) {
        return a.file.name.localeCompare(b.file.name);
      }
      return b.file.lastModified - a.file.lastModified;
    }
    // Match the Intl.Collator params used for sorting in the files app in
    // file_manager/common/js/util.js.
    const direction = sortOrder === SortOrder.A_FIRST ? 1 : -1;
    return direction *
        a.file.name.localeCompare(
            b.file.name, [],
            {usage: 'sort', numeric: true, sensitivity: 'base'});
  });

  const name = focusFile.name;
  const focusIndex =
      relatedFiles.findIndex(i => !!i.file && i.file.name === name);
  entryIndex = 0;
  if (focusIndex === -1) {
    // The focus file is no longer there i.e. might have been deleted, should be
    // missing form `currentFiles` as well.
    currentFiles.push(...relatedFiles);
    return ProcessOtherFilesResult.FOCUS_FILE_MISSING;
  } else {
    // Rotate the sorted files so focusIndex becomes index 0 such that we have
    // [focus file, ...files larger, ...files smaller].
    currentFiles.push(...relatedFiles.slice(focusIndex + 1));
    currentFiles.push(...relatedFiles.slice(0, focusIndex));
    return ProcessOtherFilesResult.FOCUS_FILE_RELEVANT;
  }
}

/**
 * Loads related files in the working directory and sends them to the guest. If
 * the focus file (currentFiles[0]) is no longer relevant i.e. is has been
 * deleted, we load files as usual.
 * @param {!FileSystemDirectoryHandle} directory
 * @param {?File} focusFile
 * @param {?FileSystemFileHandle} focusHandle
 * @param {number} localLaunchNumber
 */
async function loadOtherRelatedFiles(
    directory, focusFile, focusHandle, localLaunchNumber) {
  const processResult = await processOtherFilesInDirectory(
      directory, focusFile, localLaunchNumber);
  if (localLaunchNumber !== globalLaunchNumber ||
      processResult === ProcessOtherFilesResult.ABORT) {
    return;
  }

  const shallowCopy = [...currentFiles];
  if (processResult === ProcessOtherFilesResult.FOCUS_FILE_RELEVANT) {
    shallowCopy.shift();
    await sendSnapshotToGuest(shallowCopy, localLaunchNumber, true);
  } else {
    // If the focus file is no longer relevant, load files as normal.
    await sendSnapshotToGuest(shallowCopy, localLaunchNumber);
  }
}

/**
 * Sets state for the files opened in the current directory.
 * @param {!FileSystemDirectoryHandle} directory
 * @param {{file: !File, handle: !FileSystemFileHandle}} focusFile
 */
function setCurrentDirectory(directory, focusFile) {
  // Load currentFiles into the guest.
  currentFiles.length = 0;
  currentFiles.push({
    token: generateToken(focusFile.handle),
    file: focusFile.file,
    handle: focusFile.handle,
    inCurrentDirectory: true,
  });
  currentDirectoryHandle = directory;
  entryIndex = 0;
}

/**
 * Launch the media app with the files in the provided directory, using `handle`
 * as the initial launch entry.
 * @param {!FileSystemDirectoryHandle} directory
 * @param {!FileSystemHandle} handle
 */
async function launchWithDirectory(directory, handle) {
  const localLaunchNumber = ++globalLaunchNumber;

  let asFile;
  try {
    asFile = await getFileFromHandle(handle);
  } catch (/** @type {!DOMException} */ e) {
    console.warn(`${handle.name}: ${e.message}`);
    sendSnapshotToGuest(
        [{token: -1, file: null, handle, error: e.name}], localLaunchNumber);
    return;
  }
  // Load currentFiles into the guest.
  setCurrentDirectory(directory, asFile);
  await sendSnapshotToGuest([...currentFiles], localLaunchNumber);
  // The app is operable with the first file now.

  // Process other files in directory.
  // TODO(https://github.com/WICG/native-file-system/issues/215): Don't process
  // other files if there is only 1 file which is already loaded by
  // `sendSnapshotToGuest()` above.
  await loadOtherRelatedFiles(
      directory, asFile.file, asFile.handle, localLaunchNumber);
}

/**
 * Launch the media app with the selected files.
 * @param {!FileSystemDirectoryHandle} directory
 * @param {!Array<?FileSystemHandle>} handles
 */
async function launchWithMultipleSelection(directory, handles) {
  currentFiles.length = 0;
  for (const handle of handles) {
    if (handle && handle.kind === 'file') {
      const fileHandle = /** @type {!FileSystemFileHandle} */ (handle);
      currentFiles.push({
        token: generateToken(fileHandle),
        file: null,  // Just let sendSnapshotToGuest() "refresh" it.
        handle: fileHandle,
        // TODO(b/163285659): Enable delete/rename for multi-select files.
      });
    }
  }
  entryIndex = currentFiles.length > 0 ? 0 : -1;
  currentDirectoryHandle = directory;
  await sendFilesToGuest();
}

/**
 * Advance to another file.
 *
 * @param {number} direction How far to advance (e.g. +/-1).
 * @param {number=} currentFileToken The token of the file that
 *     direction is in reference to. If unprovided it's assumed that
 *     currentFiles[entryIndex] is the current file.
 */
async function advance(direction, currentFileToken) {
  let currIndex = entryIndex;
  if (currentFileToken) {
    const fileIndex =
        currentFiles.findIndex(fd => fd.token === currentFileToken);
    currIndex = fileIndex === -1 ? currIndex : fileIndex;
  }

  if (currentFiles.length) {
    entryIndex = (currIndex + direction) % currentFiles.length;
    if (entryIndex < 0) {
      entryIndex += currentFiles.length;
    }
  } else {
    entryIndex = -1;
  }

  await sendFilesToGuest();
}

/**
 * The launchQueue consumer. This returns a promise to help tests, but the file
 * handling API will ignore it.
 * @param {?LaunchParams} params
 * @return {!Promise<undefined>}
 */
function launchConsumer(params) {
  // The MediaApp sets `include_launch_directory = true` in its SystemAppInfo
  // struct compiled into Chrome. That means files[0] is guaranteed to be a
  // directory, with remaining launch files following it. Validate that this is
  // true and abort the launch if is is not.
  if (!params || !params.files || params.files.length < 2) {
    console.error('Invalid launch (missing files): ', params);
    return Promise.resolve();
  }

  if (assertCast(params.files[0]).kind !== 'directory') {
    console.error('Invalid launch: files[0] is not a directory: ', params);
    return Promise.resolve();
  }
  const directory =
      /** @type {!FileSystemDirectoryHandle} */ (params.files[0]);

  // With a single file selected, launch with all files in the directory as
  // navigation candidates. Otherwise, launch with all selected files (except
  // the launch directory itself) as navigation candidates.
  if (params.files.length === 2) {
    const focusEntry = assertCast(params.files[1]);
    return launchWithDirectory(directory, focusEntry);
  } else {
    return launchWithMultipleSelection(directory, params.files.slice(1));
  }
}

/**
 * Installs the handler for launch files, if window.launchQueue is available.
 */
function installLaunchHandler() {
  if (!window.launchQueue) {
    console.error('FileHandling API missing.');
    return;
  }
  window.launchQueue.setConsumer(launchConsumer);
}

installLaunchHandler();

// Make sure the guest frame has focus.
/** @type {!Element} */
const guest = assertCast(
    document.querySelector('iframe[src^="chrome-untrusted://media-app"]'));
guest.addEventListener('load', () => {
  guest.focus();
});
