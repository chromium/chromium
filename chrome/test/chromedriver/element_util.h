// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_ELEMENT_UTIL_H_
#define CHROME_TEST_CHROMEDRIVER_ELEMENT_UTIL_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"

struct Session;
class Status;
class WebView;

std::string GetElementKey();

base::Value CreateElement(const std::string& element_id);

base::Value::Dict CreateValueFrom(const WebPoint& point);

// |root_element_id| could be null when no root element is given.
Status FindElement(int interval_ms,
                   bool only_one,
                   const std::string* root_element_id,
                   Session* session,
                   WebView* web_view,
                   const base::Value::Dict& params,
                   std::unique_ptr<base::Value>* value);

Status FindShadowElement(int interval_ms,
                         bool only_one,
                         const std::string* shadow_root_id,
                         Session* session,
                         WebView* web_view,
                         const base::Value::Dict& params,
                         std::unique_ptr<base::Value>* value);

Status GetActiveElement(Session* session,
                        WebView* web_view,
                        std::unique_ptr<base::Value>* value);

Status IsElementFocused(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool* is_focused);

Status IsDocumentTypeXml(
    Session* session,
    WebView* web_view,
    bool* is_xml_document);

Status GetElementAttribute(Session* session,
                           WebView* web_view,
                           const std::string& element_id,
                           const std::string& attribute_name,
                           std::unique_ptr<base::Value>* value);

Status IsElementAttributeEqualToIgnoreCase(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const std::string& attribute_name,
    const std::string& attribute_value,
    bool* is_equal);

Status GetElementClickableLocation(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    WebPoint* location);

Status GetElementEffectiveStyle(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const std::string& property_name,
    std::string* property_value);

Status GetElementRegion(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    WebRect* rect);

Status GetElementTagName(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    std::string* name);

Status GetElementSize(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    WebSize* size);

Status IsElementDisplayed(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool ignore_opacity,
    bool* is_displayed);

Status IsElementEnabled(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool* is_enabled);

Status IsOptionElementSelected(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool* is_selected);

Status IsOptionElementTogglable(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool* is_togglable);

Status SetOptionElementSelected(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool selected);

Status ToggleOptionElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id);

// |offset| is an optional offset from the top-left of the first ClientRect
// that is returned by the element's getClientRects() function.
// If |offset| is null, the |location| returned will be the center of the
// ClientRect. If it is non-null, |location| will be offset by the specified
// value.
Status ScrollElementIntoView(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const WebPoint* offset,
    WebPoint* location);

// |element_id| refers to the element which is to be scrolled into view.
// |clickable_element_id| refers to the element needing clickable verification.
// They are usually the same, but can be different. This is useful when an image
// uses map/area. The image is scrolled, but check clickable against the area.
// If |clickable_element_id| is "", no verification will be performed.
Status ScrollElementRegionIntoView(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const WebRect& region,
    bool center,
    const std::string& clickable_element_id,
    WebPoint* location);

Status GetElementLocationInViewCenter(Session* session,
                                      WebView* web_view,
                                      const std::string& element_id,
                                      WebPoint* location);

Status GetAXNodeByElementId(Session* session,
                            WebView* web_view,
                            const std::string& element_id,
                            std::unique_ptr<base::Value>* axNode);

#endif  // CHROME_TEST_CHROMEDRIVER_ELEMENT_UTIL_H_
