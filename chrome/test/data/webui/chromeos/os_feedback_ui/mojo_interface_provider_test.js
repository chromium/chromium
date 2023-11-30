// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {getFeedbackServiceProvider, getHelpContentProvider, setFeedbackServiceProviderForTesting, setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {FeedbackServiceProviderInterface, HelpContentProviderInterface} from 'chrome://os-feedback/os_feedback_ui.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeMojoProviderTestSuite', () => {
  test('SettingGettingTestHelpContentProvider', () => {
    const fake_provider =
        /** @type {HelpContentProviderInterface} */ (
            new FakeHelpContentProvider());
    setHelpContentProviderForTesting(fake_provider);
    assertEquals(fake_provider, getHelpContentProvider());
  });

  test('GetDefaultHelpContentProvider', () => {
    const provider = getHelpContentProvider();
    assertTrue(!!provider);
  });

  test('SettingGettingTestFeedbackServiceProvider', () => {
    const fake_provider =
        /** @type {FeedbackServiceProviderInterface} */ (
            new FakeFeedbackServiceProvider());
    setFeedbackServiceProviderForTesting(fake_provider);
    assertEquals(fake_provider, getFeedbackServiceProvider());
  });

  test('GetDefaultFeedbackServiceProvider', () => {
    const provider = getFeedbackServiceProvider();
    assertTrue(!!provider);
  });
});
