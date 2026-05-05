// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary ImageData {
  // The image data, either as webp bytes or a data URL.
  (ArrayBuffer or DOMString) value;
};

// Use the <code>chrome.indigoPrivate</code> API for specific browser
// functionality that the Indigo extension needs.
interface IndigoPrivate {
  // Notifies the browser that the extension frame is ready to be rendered.
  static Promise<undefined> readyToRender();

  // Returns the original image data.
  // |PromiseValue|: imageData
  [requiredCallback]
  static Promise<ImageData> getOriginalImage();

  // Returns the replacement image data.
  // |PromiseValue|: imageData
  [requiredCallback]
  static Promise<ImageData> getReplacementImage();
};

partial interface Browser {
  static attribute IndigoPrivate indigoPrivate;
};
