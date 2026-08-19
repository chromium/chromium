// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuKeyProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;
import java.util.function.Supplier;

/** This is a helper class for app menu. */
@NullMarked
public class AppMenuUtil {
    public static class AppMenuKeyProvider implements HierarchicalMenuKeyProvider {
        @Override
        public WritableObjectPropertyKey<View.@Nullable OnClickListener> getClickListenerKey() {
            return AppMenuItemWithSubmenuProperties.CLICK_LISTENER;
        }

        @Override
        public WritableBooleanPropertyKey getEnabledKey() {
            return AppMenuItemProperties.ENABLED;
        }

        @Override
        public WritableObjectPropertyKey<View.@Nullable OnHoverListener> getHoverListenerKey() {
            return AppMenuItemProperties.HOVER_LISTENER;
        }

        @Override
        public WritableObjectPropertyKey<CharSequence> getTitleKey() {
            return AppMenuItemProperties.TITLE;
        }

        @Override
        public WritableIntPropertyKey getTitleIdKey() {
            return AppMenuItemProperties.TITLE_ID;
        }

        @Override
        public WritableObjectPropertyKey<View.OnKeyListener> getKeyListenerKey() {
            return AppMenuItemProperties.KEY_LISTENER;
        }

        @Override
        public WritableObjectPropertyKey<Supplier<List<ListItem>>> getSubmenuProviderKey() {
            return AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER;
        }

        @Override
        public WritableBooleanPropertyKey getIsHighlightedKey() {
            return AppMenuItemProperties.HAS_HOVER_BACKGROUND;
        }

        @Override
        public WritableBooleanPropertyKey getIsExpandedKey() {
            return AppMenuItemWithSubmenuProperties.IS_EXPANDED;
        }
    }
}
