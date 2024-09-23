// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Collections;

/** Unit test for {@link FormData}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FormDataTest {
    @Test
    // Tests that the factory method passes the correct parameters.
    public void testCreateFormDataSetsFieldsCorrectly() {
        int sessionId = 12345;
        String name = "SomeFormName";
        String host = "https://foo.com";
        FormData form = FormData.createFormData(sessionId, name, host, Collections.emptyList());
        assertEquals(sessionId, form.mSessionId);
        assertEquals(name, form.mName);
        assertEquals(host, form.mHost);
        assertEquals(Collections.emptyList(), form.mFields);
    }
}
