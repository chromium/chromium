// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from '../page_element_types.js';

class FileListUpdater {
  constructor() {
    $.fileDropList.replaceChildren();
  }
  appendEntry(text: string) {
    const el = document.createElement('li');
    el.replaceChildren(text);
    $.fileDropList.appendChild(el);
  }

  async addFile(file: File) {
    try {
      const text = await file.text();
      this.appendEntry(`${file.name}: ${text.length} chars read`);
    } catch (e) {
      this.appendEntry(`${file.name}: Error reading file: ${e}`);
    }
  }
  async addHandle(handle: FileSystemHandle) {
    if (handle.kind === 'directory') {
      const dir = handle as FileSystemDirectoryHandle;
      this.appendEntry(`Dropped directory: ${dir.name}`);
      for await (const [_, h] of dir.entries()) {
        this.addHandle(h);
      }
    } else if (handle.kind === 'file') {
      const file = handle as FileSystemFileHandle;
      this.addFile(await file.getFile());
    }
  }
  async addItem(item: DataTransferItem) {
    const handle =
        (await (item as any).getAsFileSystemHandle()) as FileSystemHandle;
    if (handle) {
      this.addHandle(handle);
    } else {
      const file = item.getAsFile();
      if (file) {
        this.addFile(file);
      }
    }
  }
}

// When files or directories are dropped, log the contents to `fileDropList`.
// This confirms that the web client has file access.
$.fileDrop.addEventListener('drop', (e: DragEvent) => {
  e.preventDefault();
  if (!e.dataTransfer) {
    return;
  }

  const updater = new FileListUpdater();

  if (e.dataTransfer.items) {
    [...e.dataTransfer.items].forEach((item) => {
      updater.addItem(item);
    });
  } else {
    [...e.dataTransfer.files].forEach((file) => {
      updater.addFile(file);
    });
  }
});
$.fileDrop.addEventListener('dragover', (e) => {
  e.preventDefault();
});
$.showDirectoryPicker.addEventListener('click', async () => {
  const updater = new FileListUpdater();
  try {
    const handle = (await (window as any).showDirectoryPicker()) as
        FileSystemDirectoryHandle;
    updater.addHandle(handle);
  } catch (e) {
    updater.appendEntry(`Error: ${e}`);
  }
});
