// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {confirmationPageTest} from './confirmation_page_test.js';
import {fakeHelpContentProviderTestSuite} from './fake_help_content_provider_test.js';
import {FeedbackFlowTestSuite} from './feedback_flow_test.js';
import {fileAttachmentTestSuite} from './file_attachment_test.js';
import {helpContentTestSuite} from './help_content_test.js';
import {fakeMojoProviderTestSuite} from './mojo_interface_provider_test.js';
import {searchPageTestSuite} from './search_page_test.js';
import {shareDataPageTestSuite} from './share_data_page_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('confirmationPageTest', confirmationPageTest);
runSuite('fakeHelpContentProviderTest', fakeHelpContentProviderTestSuite);
runSuite('fakeMojoProviderTest', fakeMojoProviderTestSuite);
runSuite('feedbackFlowTest', FeedbackFlowTestSuite);
runSuite('fileAttachmentTest', fileAttachmentTestSuite);
runSuite('helpContentTest', helpContentTestSuite);
runSuite('searchPageTest', searchPageTestSuite);
runSuite('shareDataPageTest', shareDataPageTestSuite);
