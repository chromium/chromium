// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_ELEMENT_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_ELEMENT_COMMANDS_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"

namespace base {
class DictionaryValue;
class Value;
}

struct Session;
class Status;
class Timeout;
class WebView;

typedef base::Callback<Status(Session* session,
                              WebView* web_view,
                              const std::string&,
                              const base::DictionaryValue&,
                              std::unique_ptr<base::Value>*)>
    ElementCommand;

// Execute a command on a specific element.
Status ExecuteElementCommand(
    const ElementCommand& command,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    std::unique_ptr<base::Value>* value,
    Timeout* timeout);

// Search for an element on the page, starting from the given element.
Status ExecuteFindChildElement(int interval_ms,
                               Session* session,
                               WebView* web_view,
                               const std::string& element_id,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value);

// Search for multiple elements on the page, starting from the given element.
Status ExecuteFindChildElements(int interval_ms,
                                Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value);

// Click on the element.
Status ExecuteClickElement(Session* session,
                           WebView* web_view,
                           const std::string& element_id,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value);

// Touch click on the element.
Status ExecuteTouchSingleTap(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value);

// Double tap on the element.
Status ExecuteTouchDoubleTap(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value);

// Long press on the element.
Status ExecuteTouchLongPress(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value);

// Touch flick starting on the element.
Status ExecuteFlick(Session* session,
                    WebView* web_view,
                    const std::string& element_id,
                    const base::DictionaryValue& params,
                    std::unique_ptr<base::Value>* value);

// Clear a TEXTAREA or text INPUT element's value.
Status ExecuteClearElement(Session* session,
                           WebView* web_view,
                           const std::string& element_id,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value);

// Send a sequence of key strokes to an element.
Status ExecuteSendKeysToElement(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value);

// Submit a form element.
Status ExecuteSubmitElement(Session* session,
                            WebView* web_view,
                            const std::string& element_id,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value);

// Returns the text of a given element.
Status ExecuteGetElementText(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value);

// Returns the value of a given element.
Status ExecuteGetElementValue(Session* session,
                              WebView* web_view,
                              const std::string& element_id,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value);

// Returns the value of a given element property.
Status ExecuteGetElementProperty(Session* session,
                              WebView* web_view,
                              const std::string& element_id,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value);

// Returns the lower case tag name of a given element.
Status ExecuteGetElementTagName(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value);

Status ExecuteIsElementSelected(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value);

Status ExecuteIsElementEnabled(Session* session,
                               WebView* web_view,
                               const std::string& element_id,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value);

Status ExecuteIsElementDisplayed(Session* session,
                                 WebView* web_view,
                                 const std::string& element_id,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value);

// Returns the location of a given element in page coordinates.
Status ExecuteGetElementLocation(Session* session,
                                 WebView* web_view,
                                 const std::string& element_id,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value);

Status ExecuteGetElementRect(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value);

// Returns the location of a given element in client coordinates, after
// scrolling it into view.
Status ExecuteGetElementLocationOnceScrolledIntoView(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    std::unique_ptr<base::Value>* value);

Status ExecuteGetElementSize(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value);

Status ExecuteGetElementAttribute(Session* session,
                                  WebView* web_view,
                                  const std::string& element_id,
                                  const base::DictionaryValue& params,
                                  std::unique_ptr<base::Value>* value);

// Returns the effective style for a given property of the specified element.
Status ExecuteGetElementValueOfCSSProperty(Session* session,
                                           WebView* web_view,
                                           const std::string& element_id,
                                           const base::DictionaryValue& params,
                                           std::unique_ptr<base::Value>* value);

// Returns whether the two given elements are equivalent.
Status ExecuteElementEquals(Session* session,
                            WebView* web_view,
                            const std::string& element_id,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value);

// Retrieves a screenshot of a specific element
Status ExecuteElementScreenshot(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value);

#endif  // CHROME_TEST_CHROMEDRIVER_ELEMENT_COMMANDS_H_
