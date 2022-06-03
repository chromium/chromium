// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

/**
 * A preference that takes value from a specified list of objects, presented as a dropdown.
 */
public class SpinnerPreference extends Preference {
    private Spinner mSpinner;
    private ArrayAdapter<Object> mAdapter;
    private int mSelectedIndex;
    private final boolean mSingleLine;

    /**
     * Constructor for inflating from XML.
     */
    public SpinnerPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.SpinnerPreference);
        mSingleLine = a.getBoolean(R.styleable.SpinnerPreference_singleLine, false);
        a.recycle();
        if (mSingleLine) {
            setLayoutResource(R.layout.preference_spinner_single_line);
        } else {
            setLayoutResource(R.layout.preference_spinner);
        }
    }

    /**
     * Provides a list of arbitrary objects to be shown in the spinner. Visually, each option will
     * be presented as its toString() text. Alternative to {@link #setAdapter(ArrayAdapter, int)}.
     *
     * @param options The options to be shown in the spinner.
     * @param selectedIndex Index of the initially selected option.
     */
    public void setOptions(Object[] options, int selectedIndex) {
        int itemLayout;
        if (mSingleLine) {
            itemLayout = R.layout.preference_spinner_single_line_item;
        } else {
            itemLayout = android.R.layout.simple_spinner_item;
        }
        mAdapter = new ArrayAdapter<>(getContext(), itemLayout, options);
        mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mSelectedIndex = selectedIndex;
    }

    /**
     * Returns the Spinner instance for introspection during tests.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public Spinner getSpinnerForTesting() {
        return mSpinner;
    }

    /**
     * Provides an adapter containing objects to be shown in the spinner. Alternatively, a list of
     * objects to be shown may be provided in {@link #setOptions(Object[], int)}. It is expected
     * that only one of these methods will be called.
     *
     * @param arrayAdapter  The array adapter to use.
     * @param selectedIndex The index of the selected item.
     */
    public void setAdapter(ArrayAdapter<Object> arrayAdapter, int selectedIndex) {
        mAdapter = arrayAdapter;
        mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mSelectedIndex = selectedIndex;
    }

    /**
     * @return The currently selected option.
     */
    public Object getSelectedOption() {
        if (mSpinner == null) {
            // Use the adapter directly if the view hasn't been created yet.
            return mAdapter.getItem(mSelectedIndex);
        }
        return mSpinner.getSelectedItem();
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ((TextView) holder.findViewById(R.id.title)).setText(getTitle());
        mSpinner = (Spinner) holder.findViewById(R.id.spinner);
        mSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                mSelectedIndex = position;
                if (getOnPreferenceChangeListener() != null) {
                    getOnPreferenceChangeListener().onPreferenceChange(
                            SpinnerPreference.this, getSelectedOption());
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                // No callback. Only update listeners when an actual option is selected.
            }
        });

        // Screen readers notice the setAdapter() call and announce it. We do not want the spinner
        // to be announced every time the view is bound (e.g. when the user scrolls away from it
        // and then back). Therefore, only update the adapter if it has actually changed.
        if (mSpinner.getAdapter() != mAdapter) {
            mSpinner.setAdapter(mAdapter);
        }
        mSpinner.setSelection(mSelectedIndex);
    }
}
