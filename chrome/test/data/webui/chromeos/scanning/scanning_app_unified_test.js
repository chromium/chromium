// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug/1004256): Use es6 module for mojo binding.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://scanning/file_path.mojom-lite.js';
import 'chrome://scanning/scanning.mojom-lite.js';

import {colorModeSelectTest} from './color_mode_select_test.js';
import {fileTypeSelectTest} from './file_type_select_test.js';
import {loadingPageTest} from './loading_page_test.js';
import {pageSizeSelectTest} from './page_size_select_test.js';
import {resolutionSelectTest} from './resolution_select_test.js';
import {scanDoneSectionTest} from './scan_done_section_test.js';
import {scanPreviewTest} from './scan_preview_test.js';
import {scanToSelectTest} from './scan_to_select_test.js';
import {scannerSelectTest} from './scanner_select_test.js';
import {scanningAppTest} from './scanning_app_test.js';
import {sourceSelectTest} from './source_select_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('ColorModeSelect', colorModeSelectTest);
runSuite('FileTypeSelect', fileTypeSelectTest);
runSuite('LoadingPage', loadingPageTest);
runSuite('PageSizeSelect', pageSizeSelectTest);
runSuite('ResolutionSelect', resolutionSelectTest);
runSuite('ScanApp', scanningAppTest);
runSuite('ScanDoneSection', scanDoneSectionTest);
runSuite('ScannerSelect', scannerSelectTest);
runSuite('ScanPreview', scanPreviewTest);
runSuite('ScanToSelect', scanToSelectTest);
runSuite('SourceSelect', sourceSelectTest);
