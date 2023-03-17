// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MojoFileSystemAccessTest} from './mojo_file_system_access_test.mojom-webui.js';

// Expose the interface for browsertest EvalJs.
Object.assign(window, {MojoFileSystemAccessTest});
