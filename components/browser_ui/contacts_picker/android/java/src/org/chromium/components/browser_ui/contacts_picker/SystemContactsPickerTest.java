// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.app.Activity;
import android.content.Intent;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ContactsFetcher;
import org.chromium.content_public.browser.ContactsPickerListener;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.lang.ref.WeakReference;
import java.util.ArrayList;

/** Tests for the System Contacts Picker integration. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ContactsPickerFeatureList.ANDROID_SYSTEM_CONTACTS_PICKER)
public class SystemContactsPickerTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ContactsPickerListener mListener;
    @Mock private ContactsPickerFeatureMap mMockFeatureMap;

    private PickerCategoryView mCategoryView;
    private Activity mActivity;

    private static class TestPickerAdapter extends PickerAdapter {
        @Override
        protected @Nullable String findOwnerEmail() {
            return null;
        }

        @Override
        protected void addOwnerInfoToContacts(
                ArrayList<ContactDetails> contacts, String ownerEmail) {}
    }

    @Before
    public void setUp() {
        FakeAconfigFlaggedApiDelegate fakeDelegate = new FakeAconfigFlaggedApiDelegate();
        fakeDelegate.setSystemContactsPickerEnabled(true);
        AconfigFlaggedApiDelegate.setInstanceForTesting(fakeDelegate);

        // Mock the feature map to avoid native calls.
        ContactsPickerFeatureMap.setInstanceForTesting(mMockFeatureMap);
        Mockito.doReturn(true)
                .when(mMockFeatureMap)
                .isEnabledInNative(ContactsPickerFeatureList.ANDROID_SYSTEM_CONTACTS_PICKER);

        activityTestRule.launchActivity(null);
        mActivity = activityTestRule.getActivity();

        Mockito.doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();
        Mockito.doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getContext();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ContactsFetcher fetcher = new ContactsFetcherTestImpl();
                    PickerAdapter adapter = new TestPickerAdapter();
                    mCategoryView =
                            new PickerCategoryView(
                                    mWindowAndroid,
                                    adapter,
                                    false,
                                    true,
                                    true,
                                    true,
                                    true,
                                    true,
                                    "example.com",
                                    null,
                                    fetcher);

                    // initialize() is what sets up the TopView and other UI components.
                    // We use a mock Dialog to satisfy the requirement.
                    ContactsPickerDialog mockDialog = Mockito.mock(ContactsPickerDialog.class);
                    mCategoryView.initialize(mockDialog, mListener);

                    mActivity.setContentView(mCategoryView);
                });
    }

    @Test
    @LargeTest
    public void testLaunchSystemPicker() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Verify that showCancelableIntent was called on WindowAndroid
                    // during initialization.
                    ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
                    Mockito.verify(mWindowAndroid)
                            .showCancelableIntent(
                                    intentCaptor.capture(), Mockito.any(), Mockito.any());

                    Intent intent = intentCaptor.getValue();
                    Assert.assertEquals(
                            FakeAconfigFlaggedApiDelegate.ACTION_PICK_CONTACTS, intent.getAction());
                });
    }
}
