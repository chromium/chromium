// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;

import java.util.Collections;

/** Unit test for {@link FormData}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({AndroidAutofillFeatures.ANDROID_AUTOFILL_FORWARD_IFRAME_ORIGIN_NAME})
public class FormDataTest {
    @Test
    // Tests that the factory method passes the correct parameters.
    public void testCreateFormDataSetsFieldsCorrectly() {
        int sessionId = 12345;
        String name = "SomeFormName";
        final String siteHost = "https://foo.com";
        final String frameHost = "https://frame.foo.com";
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        fieldBuilder.mOrigin = frameHost;
        FormData form =
                FormData.createFormData(
                        sessionId, name, siteHost, Collections.singletonList(fieldBuilder.build()));
        assertEquals(sessionId, form.mSessionId);
        assertEquals(name, form.mName);
        assertEquals(siteHost, form.mHost);
        assertEquals(frameHost, form.mFields.get(0).mOrigin);
    }
}
