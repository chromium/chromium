// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.transition.TransitionManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes.OptionalSectionType;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

@NullMarked
public class ExtensionsMenuViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        // We use beginDelayedTransition to smoothly animate the resulting layout
        // resizing, preventing the menu from abruptly "jumping" to its new height.
        if (key == ExtensionsMenuProperties.IS_ZERO_STATE
                || key == ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE
                || key == ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE) {
            TransitionManager.beginDelayedTransition((ViewGroup) view);
        }

        if (key == ExtensionsMenuProperties.CLOSE_CLICK_LISTENER) {
            View closeButton = view.findViewById(R.id.extensions_menu_close_button);
            closeButton.setOnClickListener(
                    model.get(ExtensionsMenuProperties.CLOSE_CLICK_LISTENER));
            ViewCompat.setTooltipText(closeButton, view.getContext().getString(R.string.close));
        } else if (key == ExtensionsMenuProperties.DISCOVER_EXTENSIONS_VISIBLE) {
            view.findViewById(R.id.extensions_menu_discover_extensions_button)
                    .setVisibility(
                            model.get(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_VISIBLE)
                                    ? View.VISIBLE
                                    : View.GONE);
        } else if (key == ExtensionsMenuProperties.DISCOVER_EXTENSIONS_CLICK_LISTENER) {
            view.findViewById(R.id.extensions_menu_discover_extensions_button)
                    .setOnClickListener(
                            model.get(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_CLICK_LISTENER));
            View zeroStateDiscoverButton = view.findViewById(R.id.btn_open_store);
            if (zeroStateDiscoverButton != null) {
                zeroStateDiscoverButton.setOnClickListener(
                        model.get(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_CLICK_LISTENER));
            }
        } else if (key == ExtensionsMenuProperties.IS_ZERO_STATE) {
            boolean isZeroState = model.get(ExtensionsMenuProperties.IS_ZERO_STATE);
            View activeStateView = view.findViewById(R.id.extensions_menu_active_state);
            View zeroStateView = view.findViewById(R.id.extensions_menu_zero_state);

            if (isZeroState) {
                activeStateView.setVisibility(View.GONE);
                zeroStateView.setVisibility(View.VISIBLE);
            } else {
                activeStateView.setVisibility(View.VISIBLE);
                zeroStateView.setVisibility(View.GONE);
            }
        } else if (key == ExtensionsMenuProperties.MANAGE_EXTENSIONS_CLICK_LISTENER) {
            view.findViewById(R.id.extensions_menu_manage_extensions_button)
                    .setOnClickListener(
                            model.get(ExtensionsMenuProperties.MANAGE_EXTENSIONS_CLICK_LISTENER));
        } else if (key == ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE) {
            view.findViewById(R.id.extensions_menu_site_settings_toggle)
                    .setVisibility(
                            model.get(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE)
                                    ? View.VISIBLE
                                    : View.GONE);
        } else if (key == ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED) {
            MaterialSwitchWithText toggle =
                    view.findViewById(R.id.extensions_menu_site_settings_toggle);
            toggle.setChecked(model.get(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED));
        } else if (key == ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CLICK_LISTENER) {
            MaterialSwitchWithText toggle =
                    view.findViewById(R.id.extensions_menu_site_settings_toggle);
            toggle.setOnCheckedChangeListener(
                    model.get(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CLICK_LISTENER));
        } else if (key == ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE) {
            List<ExtensionsMenuTypes.HostAccessRequest> requests =
                    model.get(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS);
            updateSectionVisibilities(
                    view, model.get(ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE), requests);
        } else if (key == ExtensionsMenuProperties.HOST_ACCESS_REQUESTS) {
            List<ExtensionsMenuTypes.HostAccessRequest> requests =
                    model.get(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS);
            // Update visibility (in case the list became empty or non-empty)
            updateSectionVisibilities(
                    view, model.get(ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE), requests);

            // Repopulate the list
            LinearLayout listContainer = view.findViewById(R.id.extensions_request_access_list);
            listContainer.removeAllViews();
            if (requests != null) {
                LayoutInflater inflater = LayoutInflater.from(listContainer.getContext());
                for (ExtensionsMenuTypes.HostAccessRequest request : requests) {
                    View itemView =
                            inflater.inflate(
                                    R.layout.extensions_request_access_item, listContainer, false);
                    populateRequestView(itemView, model, request);
                    listContainer.addView(itemView);
                }
            }
        } else if (key == ExtensionsMenuProperties.RELOAD_CLICK_LISTENER) {
            View reloadButton = view.findViewById(R.id.extensions_menu_reload_button);
            reloadButton.setOnClickListener(
                    model.get(ExtensionsMenuProperties.RELOAD_CLICK_LISTENER));
        } else if (key == ExtensionsMenuProperties.SITE_SETTINGS_LABEL) {
            MaterialSwitchWithText toggle =
                    view.findViewById(R.id.extensions_menu_site_settings_toggle);
            toggle.setText(model.get(ExtensionsMenuProperties.SITE_SETTINGS_LABEL));
        }
    }

    private static void updateSectionVisibilities(
            View view,
            int optionalSectionType,
            List<ExtensionsMenuTypes.HostAccessRequest> requests) {
        View reloadSection = view.findViewById(R.id.extensions_menu_reload_section);
        int expectedVisibility =
                (optionalSectionType == OptionalSectionType.RELOAD_PAGE) ? View.VISIBLE : View.GONE;
        if (reloadSection.getVisibility() != expectedVisibility) {
            reloadSection.setVisibility(expectedVisibility);
        }

        View requestsSection = view.findViewById(R.id.extensions_request_access_section);
        boolean hasRequests = requests != null && !requests.isEmpty();
        int requestsSectionExpectedVisibility =
                (optionalSectionType == OptionalSectionType.HOST_ACCESS_REQUESTS && hasRequests)
                        ? View.VISIBLE
                        : View.GONE;
        if (requestsSection.getVisibility() != requestsSectionExpectedVisibility) {
            requestsSection.setVisibility(requestsSectionExpectedVisibility);
        }
    }

    private static void populateRequestView(
            View itemView, PropertyModel model, ExtensionsMenuTypes.HostAccessRequest request) {
        Context context = itemView.getContext();

        // Set Icon and Name
        ImageView iconView = itemView.findViewById(R.id.extension_icon);
        if (request.extensionIcon != null) {
            iconView.setImageBitmap(request.extensionIcon);
        }
        TextView nameView = itemView.findViewById(R.id.extension_name);
        nameView.setText(request.extensionName);

        // Setup Allow Button
        View allowButton = itemView.findViewById(R.id.allow_button);
        ViewCompat.setTooltipText(
                allowButton,
                context.getString(
                        R.string.extensions_menu_requests_access_section_allow_button_tooltip));
        allowButton.setContentDescription(
                context.getString(
                        R.string
                                .extensions_menu_requests_access_section_allow_button_accessible_name,
                        request.extensionName));
        allowButton.setOnClickListener(
                v -> {
                    Callback<String> listener =
                            model.get(ExtensionsMenuProperties.ALLOW_EXTENSION_CLICK_LISTENER);
                    if (listener != null) {
                        listener.onResult(request.extensionId);
                    }
                });

        // Setup Dismiss Button
        View dismissButton = itemView.findViewById(R.id.dismiss_button);
        ViewCompat.setTooltipText(
                dismissButton,
                context.getString(
                        R.string.extensions_menu_requests_access_section_dismiss_button_tooltip));
        dismissButton.setContentDescription(
                context.getString(
                        R.string
                                .extensions_menu_requests_access_section_dismiss_button_accessible_name,
                        request.extensionName));
        dismissButton.setOnClickListener(
                v -> {
                    Callback<String> listener =
                            model.get(ExtensionsMenuProperties.DISMISS_EXTENSION_CLICK_LISTENER);
                    if (listener != null) {
                        listener.onResult(request.extensionId);
                    }
                });
    }
}
