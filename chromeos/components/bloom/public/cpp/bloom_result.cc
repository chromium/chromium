// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/public/cpp/bloom_result.h"

namespace chromeos {
namespace bloom {

// BloomElement.

const BloomImageElement* BloomElement::AsImage() const {
  DCHECK(false);
  return nullptr;
}

const BloomMathElement* BloomElement::AsMath() const {
  DCHECK(false);
  return nullptr;
}

const BloomSourceElement* BloomElement::AsSource() const {
  DCHECK(false);
  return nullptr;
}

const BloomTextElement* BloomElement::AsText() const {
  DCHECK(false);
  return nullptr;
}

const BloomTitleElement* BloomElement::AsTitle() const {
  DCHECK(false);
  return nullptr;
}

const BloomVideoElement* BloomElement::AsVideo() const {
  DCHECK(false);
  return nullptr;
}

std::string ToString(BloomElement::Type type) {
#define CASE(name)               \
  case BloomElement::Type::name: \
    return #name;

  switch (type) {
    CASE(kImage);
    CASE(kMath);
    CASE(kSource);
    CASE(kText);
    CASE(kTitle);
    CASE(kVideo);
  }

#undef CASE
}

// Image element.

BloomImageElement::BloomImageElement() = default;
BloomImageElement::BloomImageElement(BloomImageElement&&) = default;
BloomImageElement& BloomImageElement::operator=(BloomImageElement&&) = default;
BloomImageElement::~BloomImageElement() = default;

BloomElement::Type BloomImageElement::type() const {
  return Type::kImage;
}

const BloomImageElement* BloomImageElement::AsImage() const {
  return this;
}

// Source element.

BloomSourceElement::BloomSourceElement() = default;
BloomSourceElement::BloomSourceElement(BloomSourceElement&&) = default;
BloomSourceElement& BloomSourceElement::operator=(BloomSourceElement&&) =
    default;
BloomSourceElement::~BloomSourceElement() = default;

BloomElement::Type BloomSourceElement::type() const {
  return Type::kSource;
}

const BloomSourceElement* BloomSourceElement::AsSource() const {
  return this;
}

// Math element.

BloomMathElement::BloomMathElement() = default;
BloomMathElement::BloomMathElement(BloomMathElement&&) = default;
BloomMathElement& BloomMathElement::operator=(BloomMathElement&&) = default;
BloomMathElement::~BloomMathElement() = default;

BloomElement::Type BloomMathElement::type() const {
  return Type::kMath;
}

const BloomMathElement* BloomMathElement::AsMath() const {
  return this;
}

// Text element.

BloomTextElement::BloomTextElement() = default;
BloomTextElement::BloomTextElement(BloomTextElement&&) = default;
BloomTextElement& BloomTextElement::operator=(BloomTextElement&&) = default;
BloomTextElement::~BloomTextElement() = default;

BloomElement::Type BloomTextElement::type() const {
  return Type::kText;
}

const BloomTextElement* BloomTextElement::AsText() const {
  return this;
}

// Title element.

BloomTitleElement::BloomTitleElement() = default;
BloomTitleElement::BloomTitleElement(BloomTitleElement&&) = default;
BloomTitleElement& BloomTitleElement::operator=(BloomTitleElement&&) = default;
BloomTitleElement::~BloomTitleElement() = default;

BloomElement::Type BloomTitleElement::type() const {
  return Type::kTitle;
}

const BloomTitleElement* BloomTitleElement::AsTitle() const {
  return this;
}

// Video element.

BloomVideoElement::BloomVideoElement() = default;
BloomVideoElement::BloomVideoElement(BloomVideoElement&&) = default;
BloomVideoElement& BloomVideoElement::operator=(BloomVideoElement&&) = default;
BloomVideoElement::~BloomVideoElement() = default;

BloomElement::Type BloomVideoElement::type() const {
  return Type::kVideo;
}

const BloomVideoElement* BloomVideoElement::AsVideo() const {
  return this;
}

// BloomResultEntry.

BloomResultEntry::BloomResultEntry() = default;
BloomResultEntry::~BloomResultEntry() = default;

const BloomQuestionAndAnswerEntry* BloomResultEntry::AsQuestionAndAnswer()
    const {
  DCHECK(false);
  return nullptr;
}

const BloomExplainerEntry* BloomResultEntry::AsExplainer() const {
  DCHECK(false);
  return nullptr;
}

const BloomVideoEntry* BloomResultEntry::AsVideo() const {
  DCHECK(false);
  return nullptr;
}

const BloomWebResultEntry* BloomResultEntry::AsWebResult() const {
  DCHECK(false);
  return nullptr;
}

std::string ToString(BloomResultEntry::Type type) {
#define CASE(name)                   \
  case BloomResultEntry::Type::name: \
    return #name;

  switch (type) {
    CASE(kQuestionAndAnswer);
    CASE(kExplainer);
    CASE(kVideo);
    CASE(kWebResult);
  }

#undef CASE
}

// Question and Answer.

BloomQuestionAndAnswerEntry::BloomQuestionAndAnswerEntry() = default;
BloomQuestionAndAnswerEntry::~BloomQuestionAndAnswerEntry() = default;

BloomResultEntry::Type BloomQuestionAndAnswerEntry::type() const {
  return Type::kQuestionAndAnswer;
}

const BloomQuestionAndAnswerEntry*
BloomQuestionAndAnswerEntry::AsQuestionAndAnswer() const {
  return this;
}

// Explainer.

BloomExplainerEntry::BloomExplainerEntry() = default;
BloomExplainerEntry::~BloomExplainerEntry() = default;

BloomResultEntry::Type BloomExplainerEntry::type() const {
  return Type::kExplainer;
}

const BloomExplainerEntry* BloomExplainerEntry::AsExplainer() const {
  return this;
}

// Video.

BloomVideoEntry::BloomVideoEntry() = default;
BloomVideoEntry::~BloomVideoEntry() = default;

BloomResultEntry::Type BloomVideoEntry::type() const {
  return Type::kVideo;
}

const BloomVideoEntry* BloomVideoEntry::AsVideo() const {
  return this;
}

// WebResult.

BloomWebResultEntry::BloomWebResultEntry() = default;
BloomWebResultEntry::~BloomWebResultEntry() = default;

BloomResultEntry::Type BloomWebResultEntry::type() const {
  return Type::kWebResult;
}

const BloomWebResultEntry* BloomWebResultEntry::AsWebResult() const {
  return this;
}

BloomResultSection::BloomResultSection() = default;
BloomResultSection::BloomResultSection(BloomResultSection&&) = default;
BloomResultSection& BloomResultSection::operator=(BloomResultSection&&) =
    default;
BloomResultSection::~BloomResultSection() = default;

BloomResult::BloomResult() = default;
BloomResult::BloomResult(BloomResult&&) = default;
BloomResult& BloomResult::operator=(BloomResult&&) = default;
BloomResult::~BloomResult() = default;

}  // namespace bloom
}  // namespace chromeos
