// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ComposeboxFile} from 'chrome://new-tab-page/lazy_load.js';
import {FileUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';

export function createComposeboxFile(
    index: number, override: Partial<ComposeboxFile> = {}): ComposeboxFile {
  return Object.assign(
      {
        name: `file${index}`,
        type: 'application/pdf',
        objectUrl: null,
        dataUrl: null,
        uuid: `${index}`,
        status: FileUploadStatus.kUploadSuccessful,
        url: null,
        file: null,
        tabId: null,
        isDeletable: true,
      },
      override);
}
