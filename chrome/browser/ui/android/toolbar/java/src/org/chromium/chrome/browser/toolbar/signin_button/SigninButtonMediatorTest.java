// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.signin_button;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link SigninButtonMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class SigninButtonMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Mock private MonotonicObservableSupplier<Profile> mProfileSupplier;
    private SigninButtonMediator mMediator;

    @Before
    public void setUp() {
        mMediator = new SigninButtonMediator(mProfileSupplier);
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mProfileSupplier).removeObserver(org.mockito.ArgumentMatchers.any());
    }
}
