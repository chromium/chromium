// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class EdgeToEdgeBottomChinViewBinder {
    static void bind(
            PropertyModel model,
            EdgeToEdgeBottomChinSceneLayer sceneLayer,
            PropertyKey propertyKey) {
        if (EdgeToEdgeBottomChinProperties.Y_OFFSET == propertyKey) {
            sceneLayer.setYOffset(model.get(EdgeToEdgeBottomChinProperties.Y_OFFSET));
        } else if (EdgeToEdgeBottomChinProperties.HEIGHT == propertyKey) {
            sceneLayer.setHeight(model.get(EdgeToEdgeBottomChinProperties.HEIGHT));
        } else if (EdgeToEdgeBottomChinProperties.IS_VISIBLE == propertyKey) {
            sceneLayer.setIsVisible(model.get(EdgeToEdgeBottomChinProperties.IS_VISIBLE));
        } else if (EdgeToEdgeBottomChinProperties.COLOR == propertyKey) {
            sceneLayer.setColor(model.get(EdgeToEdgeBottomChinProperties.COLOR));
        } else {
            assert false : "Unhandled property detected in EdgeToEdgeBottomChinViewBinder!";
        }
    }

    // TODO(crbug.com/345383907) Move #bind logic to the compositor MCP method
    static void bindCompositorMCP(
            PropertyModel model,
            EdgeToEdgeBottomChinSceneLayer sceneLayer,
            PropertyKey propertyKey) {
        assert propertyKey == null;
    }
}
