// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link SigninButtonMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class SigninButtonMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    private Context mContext;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.alwaysNull();
    private SigninButtonMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mModel = new PropertyModel.Builder(SigninButtonProperties.ALL_KEYS).build();
        mMediator = new SigninButtonMediator(mContext, mModel, mProfileSupplier);
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        assertFalse(mProfileSupplier.hasObservers());
    }

    @Test
    public void testUpdateButtonVisibility() {
        mMediator.updateButtonVisibility(true);
        assertTrue(mModel.get(SigninButtonProperties.SHOW_BUTTON));

        mMediator.updateButtonVisibility(false);
        assertFalse(mModel.get(SigninButtonProperties.SHOW_BUTTON));
    }
}
