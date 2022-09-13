// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.browser;

/**
 * The Java-side implementations of paint_preview_base_service.cc should implement this interface.
 * Provides a method for accessing the native PaintPreviewBaseService.
 */
public interface NativePaintPreviewServiceProvider {
    long getNativeBaseService();
}
