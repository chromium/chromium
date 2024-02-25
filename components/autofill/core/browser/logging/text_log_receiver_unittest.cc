// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/text_log_receiver.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(TextLogReceiver, IntegrationTest) {
  std::optional<base::Value> input = base::JSONReader::Read(
      R"(
    {
      "type": "element",
      "value": "div",
      "children": [
        // The first two children generate "First line\n".
        {
          "type": "text",
          "value": "First line",
        },
        {
          "type": "element",
          "value": "br",
        },
        // This generates "Line in div\n".
        {
          "type": "element",
          "value": "div",
          "children": [
            {
              "type": "text",
              "value": "Line in div",
            },
          ],
        },
        // A fragment is just inlined.
        {
          "type": "fragment",
          "children": [
            {
              "type": "text",
              "value": "Line in fragment",
            },
            {
              "type": "element",
              "value": "br",
            },
          ],
        },
        // Finally a table
        {
          "type": "element",
          "value": "table",
          "children": [
            {
              "type": "element",
              "value": "tr",
              "children": [
                {
                  "type": "element",
                  "value": "td",
                  "children": [
                    {
                      "type": "text",
                      "value": "Cell 1",
                    },
                  ],
                },
                {
                  "type": "element",
                  "value": "td",
                  "children": [
                    {
                      "type": "text",
                      "value": "Cell 2",
                    },
                  ],
                }
              ],
            },
            {
              "type": "element",
              "value": "tr",
              "children": [
                {
                  "type": "element",
                  "value": "td",
                  "children": [
                    {
                      "type": "text",
                      "value": "Cell 3",
                    },
                  ],
                },
              ],
            },
          ],
        },
      ]
    }
  )",
      base::JSON_PARSE_CHROMIUM_EXTENSIONS | base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(input);
  TextLogReceiver receiver;
  std::string result = receiver.LogEntryToText(input->GetDict());
  EXPECT_EQ(
      "First line\n"
      "Line in div\n"
      "Line in fragment\n"
      "Cell 1 Cell 2 \n"  // Trailing space from <td>
      "Cell 3 \n"         // This \n is from the <tr>.
      "\n",               // This \n is from the wrapping <div>.
      result);
}

}  // namespace autofill