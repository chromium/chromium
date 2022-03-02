// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.form;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.AssistantStaticDependencies;
import org.chromium.components.autofill_assistant.R;

import java.util.ArrayList;
import java.util.List;

/** An input in a form. */
@JNINamespace("autofill_assistant")
public abstract class AssistantFormInput {
    /** Create a view associated to this input. */
    public abstract View createView(Context context, ViewGroup parent);

    // TODO(crbug.com/806868): Check if it's possible to create generic methods createList, add, etc
    // to manipulate java lists from native code, or reuse if they already exist.
    @CalledByNative
    private static List<AssistantFormCounter> createCounterList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static List<AssistantFormSelectionChoice> createChoiceList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addCounter(
            List<AssistantFormCounter> counters, AssistantFormCounter counter) {
        counters.add(counter);
    }

    @CalledByNative
    private static void addChoice(
            List<AssistantFormSelectionChoice> choices, AssistantFormSelectionChoice choice) {
        choices.add(choice);
    }

    @CalledByNative
    private static AssistantFormCounter createCounter(String label, String descriptionLine1,
            String descriptionLine2, int initialValue, int minValue, int maxValue,
            int[] allowedValues) {
        return AssistantFormCounter.create(label, descriptionLine1, descriptionLine2, initialValue,
                minValue, maxValue, allowedValues);
    }

    @CalledByNative
    private static AssistantFormSelectionChoice createChoice(String label, String descriptionLine1,
            String descriptionLine2, boolean initiallySelected) {
        return new AssistantFormSelectionChoice(
                label, descriptionLine1, descriptionLine2, initiallySelected);
    }

    @CalledByNative
    private static AssistantFormCounterInput createCounterInput(int inputIndex, String label,
            String expandText, String minimizeText, List<AssistantFormCounter> counters,
            AssistantStaticDependencies staticDependencies, int minimizedCount, long minCountersSum,
            long maxCountersSum, AssistantFormDelegate delegate) {
        return new AssistantFormCounterInput(label, expandText, minimizeText, counters,
                staticDependencies, minimizedCount, minCountersSum, maxCountersSum,
                new AssistantFormCounterInput.Delegate() {
                    @Override
                    public void onCounterChanged(int counterIndex, int value) {
                        delegate.onCounterChanged(inputIndex, counterIndex, value);
                    }

                    @Override
                    public void onLinkClicked(int link) {
                        delegate.onLinkClicked(link);
                    }
                });
    }

    @CalledByNative
    private static AssistantFormSelectionInput createSelectionInput(int inputIndex, String label,
            List<AssistantFormSelectionChoice> choices, boolean allowMultipleChoices,
            AssistantFormDelegate delegate) {
        return new AssistantFormSelectionInput(
                label, choices, allowMultipleChoices, new AssistantFormSelectionInput.Delegate() {
                    @Override
                    public void onChoiceSelectionChanged(int choiceIndex, boolean selected) {
                        delegate.onChoiceSelectionChanged(inputIndex, choiceIndex, selected);
                    }

                    @Override
                    public void onLinkClicked(int link) {
                        delegate.onLinkClicked(link);
                    }
                });
    }

    protected void hideIfEmpty(TextView view) {
        view.setVisibility(view.length() == 0 ? View.GONE : View.VISIBLE);
    }

    protected void setMinimumHeight(View view, TextView line1, TextView line2) {
        if (line1.length() == 0 && line2.length() == 0) {
            view.setMinimumHeight(view.getContext().getResources().getDimensionPixelSize(
                    R.dimen.autofill_assistant_form_line_height_1));
        } else if (line2.length() == 0) {
            view.setMinimumHeight(view.getContext().getResources().getDimensionPixelSize(
                    R.dimen.autofill_assistant_form_line_height_2));
        } else {
            view.setMinimumHeight(view.getContext().getResources().getDimensionPixelSize(
                    R.dimen.autofill_assistant_form_line_height_3));
        }
    }
}
