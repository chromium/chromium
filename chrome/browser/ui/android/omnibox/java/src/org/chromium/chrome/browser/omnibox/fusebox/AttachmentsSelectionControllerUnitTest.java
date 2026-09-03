// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.os.SystemClock;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;

/** Unit tests for {@link AttachmentsSelectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AttachmentsSelectionControllerUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    @Mock private Runnable mOnRemoveRunnable;

    private FuseboxAttachmentModelList mModelList;
    private AttachmentsSelectionController mController;

    @Before
    public void setUp() {
        mModelList = new FuseboxAttachmentModelList();
        mModelList.setComposeboxQueryControllerBridge(mComposeboxQueryControllerBridge);
        mController = new AttachmentsSelectionController(mModelList);
    }

    private void addAttachment(String title) {
        doReturn("token" + title)
                .when(mComposeboxQueryControllerBridge)
                .addFile(eq(title), any(), any());
        mModelList.add(
                FuseboxAttachment.forImage(
                        /* thumbnail= */ null,
                        title,
                        "image/jpeg",
                        new byte[] {1, 2, 3},
                        SystemClock.elapsedRealtime(),
                        FuseboxAttachmentButtonType.FILES));
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testGetItemCount() {
        assertEquals(0, mController.getItemCount());

        addAttachment("1");
        assertEquals(1, mController.getItemCount());

        addAttachment("2");
        assertEquals(2, mController.getItemCount());
    }

    @Test
    public void testSetItemState() {
        addAttachment("1");
        var attachment = mModelList.get(0);

        mController.setItemState(0, true);
        assertTrue(attachment.model.get(FuseboxAttachmentProperties.REMOVE_BUTTON_SELECTED));

        mController.setItemState(0, false);
        assertFalse(attachment.model.get(FuseboxAttachmentProperties.REMOVE_BUTTON_SELECTED));
    }

    @Test
    public void testHandleActivation() {
        addAttachment("1");
        var attachment = mModelList.get(0);

        // Setup a mock runnable for ON_REMOVE
        attachment.model.set(FuseboxAttachmentProperties.ON_REMOVE, mOnRemoveRunnable);

        mController.selectFirstItem();
        mController.handleActivation();

        verify(mOnRemoveRunnable).run();
    }
}
