// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MojoFileSystemAccessTest} from '/mojo_file_system_access_test.mojom-webui.js';

// Expose the interface for browsertest EvalJs.
window.MojoFileSystemAccessTest = MojoFileSystemAccessTest;
