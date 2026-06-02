// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/** Coordinator for the AtMemoryBottomSheet. */
@NullMarked
public class AtMemoryBottomSheetCoordinator {
    private final AtMemoryBottomSheetContent mContent;
    private final AtMemoryBottomSheetMediator mMediator;
    private final BottomSheetController mBottomSheetController;

    public static final int ITEM_TYPE_SUGGESTION = 1;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    if (mBottomSheetController.getCurrentSheetContent() != null
                            && mBottomSheetController.getCurrentSheetContent() == mContent) {
                        onDismissed();
                    }
                }
            };

    /** Delegate to receive events from the bottom sheet. */
    interface Delegate {
        void onDismissed();

        void onSuggestionClicked(AutofillSuggestion suggestion);

        void onFlyoutClicked(AutofillSuggestion suggestion);
    }

    AtMemoryBottomSheetCoordinator(
            Context context, BottomSheetController sheetController, Delegate delegate) {
        mBottomSheetController = sheetController;

        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.VISIBLE, false)
                        .build();

        ModelList modelList = new ModelList();
        mMediator = new AtMemoryBottomSheetMediator(delegate, model, modelList);

        AtMemoryBottomSheetView view = new AtMemoryBottomSheetView(context);

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
        adapter.registerType(
                ITEM_TYPE_SUGGESTION,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_suggestion_item),
                AtMemoryBottomSheetSuggestionViewBinder::bind);
        view.setRecyclerViewAdapter(adapter);

        mContent = new AtMemoryBottomSheetContent(view.getContentView(), mBottomSheetController);

        PropertyModelChangeProcessor.create(model, view, AtMemoryBottomSheetViewBinder::bind);
    }

    public void show(List<AutofillSuggestion> suggestions) {
        mMediator.setSuggestions(suggestions);

        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (!mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            onDismissed();
        }
    }

    public void hide() {
        mBottomSheetController.hideContent(mContent, /* animate= */ true);
    }

    private void onDismissed() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mMediator.onDismissed();
    }
}
