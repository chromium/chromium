// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Based on Element::isFormControlElement() (WebKit)
 * @param element A DOM element.
 * @return true if the `element` is a form control element.
 */
export function isFormControlElement(element: Element): boolean {
  const tagName = element.tagName;
  return (
      tagName === 'INPUT' || tagName === 'SELECT' || tagName === 'TEXTAREA');
}