// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../dom.js';
import * as nav from '../nav.js';
import {MimeType, ViewName} from '../type.js';

import {View} from './view.js';

/**
 * View controller for PTZ panel.
 */
export class ReviewDocument extends View {
  /**
   * @public
   */
  constructor() {
    super(ViewName.REVIEW_DOCUMENT);

    /**
     * @private {!HTMLImageElement}
     * @const
     */
    this.image_ = dom.get('#document-image', HTMLImageElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.savePdf_ = dom.get('#save-pdf-document', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.savePhoto_ = dom.get('#save-photo-document', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.retake_ = dom.get('#retake-document', HTMLButtonElement);
  }

  /**
   * @override
   */
  focus() {
    this.savePdf_.focus();
  }

  /**
   * @param {!Blob} blob
   * @return {!Promise}
   */
  async setReviewDocument(blob) {
    try {
      await new Promise((resolve, reject) => {
        this.image_.onload = () => resolve();
        this.image_.onerror = (e) =>
            reject(new Error('Failed to load review document image: ${e}'));
        this.image_.src = URL.createObjectURL(blob);
      });
    } finally {
      URL.revokeObjectURL(this.image_.src);
    }
  }

  /**
   * @return {!Promise<?MimeType>}
   */
  async startReview() {
    nav.open(ViewName.REVIEW_DOCUMENT);
    const result = await new Promise((resolve) => {
      this.savePdf_.onclick = () => resolve(MimeType.PDF);
      this.savePhoto_.onclick = () => resolve(MimeType.JPEG);
      this.retake_.onclick = () => resolve(null);
    });
    nav.close(ViewName.REVIEW_DOCUMENT);
    this.image_.src = '';
    return result;
  }
}
