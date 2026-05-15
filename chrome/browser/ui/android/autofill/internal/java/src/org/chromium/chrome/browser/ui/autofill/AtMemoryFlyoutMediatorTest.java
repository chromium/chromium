// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link AtMemoryFlyoutMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class AtMemoryFlyoutMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AtMemoryFlyoutCoordinator.Delegate mDelegate;

    private PropertyModel mModel;
    private AtMemoryFlyoutMediator mMediator;

    @Before
    public void setUp() {
        mModel =
                new PropertyModel.Builder(AtMemoryFlyoutProperties.ALL_KEYS)
                        .build();
        mMediator = new AtMemoryFlyoutMediator(mDelegate, mModel);
    }

    @Test
    public void testOnDismissed() {
        mMediator.onDismissed();
        verify(mDelegate).onDismissed();
    }
}
