// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_BLOOM_RESULT_BUILDER_H_
#define CHROMEOS_COMPONENTS_BLOOM_BLOOM_RESULT_BUILDER_H_

#include <memory>

#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "chromeos/components/bloom/public/cpp/bloom_result.h"

namespace base {
class Value;
}  // namespace base

namespace chromeos {
namespace bloom {

// Helper class that converts the Bloom server response into a formatted
// result that can be displayed in the Assistant UI.
class BloomResultBuilder {
 public:
  BloomResultBuilder();
  BloomResultBuilder(BloomResultBuilder&) = delete;
  BloomResultBuilder& operator=(BloomResultBuilder&) = delete;
  ~BloomResultBuilder();

  BloomResult Build(const base::Value& root);

 private:
  using ConstListView = base::Value::ConstListView;

  std::string GetQuery(const base::Value& root);
  BloomResultSection BuildSection(const base::Value& section);

  std::unique_ptr<BloomQuestionAndAnswerEntry> BuildQuestionAndAnswer(
      const base::Value& entry);
  std::unique_ptr<BloomExplainerEntry> BuildExplainer(const base::Value& entry);
  std::unique_ptr<BloomVideoEntry> BuildVideo(const base::Value& entry);
  std::unique_ptr<BloomWebResultEntry> BuildWebResult(const base::Value& entry);

  // This can be |nullptr| if the element is of an unknown type.
  std::unique_ptr<BloomElement> BuildElement(const base::Value& element);
  BloomImageElement BuildImageElement(const base::Value& element);
  BloomMathElement BuildMathElement(const base::Value& element);
  BloomSourceElement BuildSourceElement(const base::Value& element);
  BloomTextElement BuildTextElement(const base::Value& element);
  BloomTitleElement BuildTitleElement(const base::Value& element);
  BloomVideoElement BuildVideoElement(const base::Value& element);

  bool HasKey(base::StringPiece key, const base::Value& parent) const;
  const base::Value& FindList(base::StringPiece key_path,
                              const base::Value& parent) const;
  int FindInt(base::StringPiece key_path, const base::Value& parent) const;
  const std::string& FindString(base::StringPiece key_path,
                                const base::Value& parent) const;
  const base::Value& FindElement(const ConstListView& list, size_t index) const;
  const base::Value& FindNthElement(const base::Value& entry,
                                    size_t index) const;

  // Default values returned when a JSON field is absent.
  // We use this instead of nullptr to make the code robust to a bug on the
  // server causing it to send invalid responses.
  const base::Value default_dict_{base::Value::Type::DICTIONARY};
  const base::Value default_list_{base::Value::Type::LIST};
  const std::string default_string_;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_BLOOM_RESULT_BUILDER_H_
