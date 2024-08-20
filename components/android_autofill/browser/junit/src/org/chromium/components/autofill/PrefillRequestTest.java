// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.os.Build;
import android.util.SparseArray;
import android.view.autofill.VirtualViewFillInfo;

import androidx.annotation.RequiresApi;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
@Config(manifest = Config.NONE, minSdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
@Features.EnableFeatures({AndroidAutofillFeatures.ANDROID_AUTOFILL_BOTTOM_SHEET_WORKAROUND_NAME})
public class PrefillRequestTest {

    @Test
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testGetHintsReturnsExpectedList() {
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        String[] firstFieldPredictions = new String[] {"NAME_FIRST", "NAME_LAST"};
        field1Builder.mServerPredictions = firstFieldPredictions;
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        field2Builder.mServerPredictions = new String[] {};

        FormData formData =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Arrays.asList(field1Builder.build(), field2Builder.build()));
        PrefillRequest prefillRequest = new PrefillRequest(formData);
        SparseArray<VirtualViewFillInfo> hintResults = prefillRequest.getPrefillHints();
        String expectedFirstPredictions = "name_first,name_last";
        String expectedSecondPredictions =
                FormFieldData.getEmptyServerPredictionsString().toLowerCase();
        Assert.assertEquals(hintResults.valueAt(0).getAutofillHints()[0], expectedFirstPredictions);
        Assert.assertEquals(
                hintResults.valueAt(1).getAutofillHints()[0], expectedSecondPredictions);
    }
}
