// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;

import androidx.pdf.selection.ContextMenuComponent;
import androidx.pdf.selection.Selection;
import androidx.pdf.selection.SelectionMenuComponent;
import androidx.pdf.selection.model.TextSelection;
import androidx.pdf.view.PdfView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.selection.SelectionUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * The top-level component responsible for the setup and lifecycle of the PDF Selection MVC stack.
 */
@NullMarked
public class PdfSelectionCoordinator {
    private static final String CONTEXT_MENU_SHARE = "Share";
    private static final String CONTEXT_MENU_TRANSLATE = "Translate";
    private static final String CONTEXT_MENU_WEB_SEARCH = "Web search";

    private final Activity mActivity;
    private final PropertyModel mModel;

    public interface SelectionMenuItemPreparer {
        void prepareMenuItems(List<ContextMenuComponent> components);
    }

    public PdfSelectionCoordinator(Activity activity, PdfView pdfView) {
        mActivity = activity;
        mModel =
                new PropertyModel.Builder(PdfSelectionProperties.ALL_KEYS)
                        .with(
                                PdfSelectionProperties.SELECTION_MENU_ITEM_PREPARER,
                                components -> prepareMenuItems(pdfView, components))
                        .build();

        PropertyModelChangeProcessor.create(mModel, pdfView, PdfSelectionViewBinder::bind);
    }

    private void prepareMenuItems(PdfView pdfView, List<ContextMenuComponent> components) {
        Selection selection = pdfView.getCurrentSelection();

        if (selection instanceof TextSelection) {
            TextSelection textSelection = (TextSelection) selection;
            components.add(
                    new SelectionMenuComponent(
                            CONTEXT_MENU_SHARE,
                            mActivity.getString(org.chromium.content.R.string.actionbar_share),
                            mActivity.getString(org.chromium.content.R.string.actionbar_share),
                            session -> {
                                SelectionUtils.share(mActivity, textSelection.getText().toString());
                                return null;
                            }));
            components.add(
                    new SelectionMenuComponent(
                            CONTEXT_MENU_WEB_SEARCH,
                            mActivity.getString(org.chromium.content.R.string.actionbar_web_search),
                            mActivity.getString(org.chromium.content.R.string.actionbar_web_search),
                            session -> {
                                SelectionUtils.webSearch(
                                        mActivity, textSelection.getText().toString());
                                return null;
                            }));

            components.add(
                    new SelectionMenuComponent(
                            CONTEXT_MENU_TRANSLATE,
                            mActivity.getString(
                                    org.chromium.chrome.browser.pdf.R.string.actionbar_translate),
                            mActivity.getString(
                                    org.chromium.chrome.browser.pdf.R.string.actionbar_translate),
                            session -> {
                                SelectionUtils.translate(
                                        mActivity, textSelection.getText().toString());
                                return null;
                            }));
        }
    }
}
