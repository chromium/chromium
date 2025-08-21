// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.core.Is.is;

import static org.chromium.base.test.transit.Condition.fulfilled;
import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.base.test.transit.SimpleConditions.instrumentationThreadCondition;
import static org.chromium.base.test.transit.SimpleConditions.instrumentationThreadConditionWithResult;

import android.view.View;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.pdf.PdfCoordinator;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.test.R;

/** The screen that shows a native PDF page within a tabbed activity. */
public class PdfCtaPageStation extends CtaPageStation {

    public final Element<PdfPage> pdfNativePageElement;

    public final Element<PdfCoordinator> pdfCoordinatorElement;
    public ViewElement<View> pdfFragmentViewElement;

    public static Builder<PdfCtaPageStation> newBuilder() {
        return new Builder<>(PdfCtaPageStation::new);
    }

    protected PdfCtaPageStation(Config config) {
        super(config);

        pdfNativePageElement =
                declareEnterConditionAsElement(
                        new NativePageCondition<>(PdfPage.class, loadedTabElement));
        pdfCoordinatorElement =
                declareEnterConditionAsElement(
                        instrumentationThreadConditionWithResult(
                                "PDF coordinator",
                                pdfNativePageElement,
                                pdfPage -> fulfilled().withResult(pdfPage.mPdfCoordinator)));
        declareEnterCondition(
                instrumentationThreadCondition(
                        "PDF document is loaded successfully",
                        pdfCoordinatorElement,
                        pdfCoordinator ->
                                whether(
                                        pdfCoordinator
                                                .mChromePdfViewerFragment
                                                .mIsLoadDocumentSuccess)));
        declareNoView(withId(R.id.progress_bar));
        declareElementFactory(
                pdfCoordinatorElement,
                delayedElements -> {
                    pdfFragmentViewElement =
                            delayedElements.declareView(
                                    is(pdfCoordinatorElement.value().getView()));
                });
    }
}
