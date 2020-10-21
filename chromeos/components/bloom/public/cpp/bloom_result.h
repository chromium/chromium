// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_RESULT_H_
#define CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_RESULT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "url/gurl.h"

namespace chromeos {
namespace bloom {

struct BloomImageElement;
struct BloomMathElement;
struct BloomSourceElement;
struct BloomTextElement;
struct BloomTitleElement;
struct BloomVideoElement;

// Abstract base class representing individual elements (a text, a video, an
// image, ...) that are used to compose the Bloom response entries.
struct COMPONENT_EXPORT(BLOOM) BloomElement {
  BloomElement() = default;
  BloomElement(const BloomElement&) = delete;
  BloomElement& operator=(const BloomElement&) = delete;
  BloomElement(BloomElement&&) = default;
  BloomElement& operator=(BloomElement&&) = default;
  virtual ~BloomElement() = default;

  enum class Type {
    kImage,
    kMath,
    kSource,
    kText,
    kTitle,
    kVideo,
  };

  virtual Type type() const = 0;

  // Methods for checked downcasting to derived types.
  // DCHECK when called on a wrong type.

  virtual const BloomImageElement* AsImage() const;
  virtual const BloomMathElement* AsMath() const;
  virtual const BloomSourceElement* AsSource() const;
  virtual const BloomTextElement* AsText() const;
  virtual const BloomTitleElement* AsTitle() const;
  virtual const BloomVideoElement* AsVideo() const;
};

std::string COMPONENT_EXPORT(BLOOM) ToString(BloomElement::Type type);

struct COMPONENT_EXPORT(BLOOM) BloomImageElement : public BloomElement {
  BloomImageElement();
  BloomImageElement(const BloomImageElement&) = delete;
  BloomImageElement& operator=(const BloomImageElement&) = delete;
  BloomImageElement(BloomImageElement&&);
  BloomImageElement& operator=(BloomImageElement&&);
  ~BloomImageElement() override;

  // BloomResultElement implementation:
  Type type() const override;
  const BloomImageElement* AsImage() const override;

  GURL url;
  std::string description;
  int width;
  int height;
};

struct COMPONENT_EXPORT(BLOOM) BloomMathElement : public BloomElement {
  BloomMathElement();
  BloomMathElement(const BloomMathElement&) = delete;
  BloomMathElement& operator=(const BloomMathElement&) = delete;
  BloomMathElement(BloomMathElement&&);
  BloomMathElement& operator=(BloomMathElement&&);
  ~BloomMathElement() override;

  // BloomResultElement implementation:
  Type type() const override;
  const BloomMathElement* AsMath() const override;

  std::string description;
  std::string latex;
};

struct COMPONENT_EXPORT(BLOOM) BloomSourceElement : public BloomElement {
  BloomSourceElement();
  BloomSourceElement(const BloomSourceElement&) = delete;
  BloomSourceElement& operator=(const BloomSourceElement&) = delete;
  BloomSourceElement(BloomSourceElement&&);
  BloomSourceElement& operator=(BloomSourceElement&&);
  ~BloomSourceElement() override;

  // BloomResultElement implementation:
  Type type() const override;
  const BloomSourceElement* AsSource() const override;

  std::string text;
  GURL favicon_url;
  GURL url;
};

struct COMPONENT_EXPORT(BLOOM) BloomTextElement : public BloomElement {
  BloomTextElement();
  BloomTextElement(const BloomTextElement&) = delete;
  BloomTextElement& operator=(const BloomTextElement&) = delete;
  BloomTextElement(BloomTextElement&&);
  BloomTextElement& operator=(BloomTextElement&&);
  ~BloomTextElement() override;

  // BloomResultElement implementation:
  Type type() const override;
  const BloomTextElement* AsText() const override;

  std::string text;
};

struct COMPONENT_EXPORT(BLOOM) BloomTitleElement : public BloomElement {
  BloomTitleElement();
  BloomTitleElement(const BloomTitleElement&) = delete;
  BloomTitleElement& operator=(const BloomTitleElement&) = delete;
  BloomTitleElement(BloomTitleElement&&);
  BloomTitleElement& operator=(BloomTitleElement&&);
  ~BloomTitleElement() override;

  // BloomResultElement implementation:
  Type type() const override;
  const BloomTitleElement* AsTitle() const override;

  std::string text;
};

struct COMPONENT_EXPORT(BLOOM) BloomVideoElement : public BloomElement {
  BloomVideoElement();
  BloomVideoElement(const BloomVideoElement&) = delete;
  BloomVideoElement& operator=(const BloomVideoElement&) = delete;
  BloomVideoElement(BloomVideoElement&&);
  BloomVideoElement& operator=(BloomVideoElement&&);
  ~BloomVideoElement() override;

  // BloomResultElement implementation:
  Type type() const override;
  const BloomVideoElement* AsVideo() const override;

  GURL url;
  std::string title;
  std::string description;
  GURL thumbnail_url;
  std::string video_id;
  std::string start_time;
  std::string duration;
  std::string channel_title;
  std::string published_time;
  std::string number_of_likes;
  std::string number_of_views;
};

struct BloomQuestionAndAnswerEntry;
struct BloomExplainerEntry;
struct BloomVideoEntry;
struct BloomWebResultEntry;

// Abstract base class representing the different entries in the Bloom response.
// Each entry contains a single answer, and is composed of multiple elements.
struct COMPONENT_EXPORT(BLOOM) BloomResultEntry {
  BloomResultEntry();
  BloomResultEntry(const BloomResultEntry&) = delete;
  BloomResultEntry& operator=(const BloomResultEntry&) = delete;
  virtual ~BloomResultEntry();

  enum class Type {
    kQuestionAndAnswer,
    kExplainer,
    kVideo,
    kWebResult,
  };

  virtual Type type() const = 0;

  // Methods for checked downcasting to derived types.
  // DCHECK when called on a wrong type.

  virtual const BloomQuestionAndAnswerEntry* AsQuestionAndAnswer() const;
  virtual const BloomExplainerEntry* AsExplainer() const;
  virtual const BloomVideoEntry* AsVideo() const;
  virtual const BloomWebResultEntry* AsWebResult() const;
};

std::string COMPONENT_EXPORT(BLOOM) ToString(BloomResultEntry::Type type);

struct COMPONENT_EXPORT(BLOOM) BloomQuestionAndAnswerEntry
    : public BloomResultEntry {
  BloomQuestionAndAnswerEntry();
  ~BloomQuestionAndAnswerEntry() override;

  // BloomResultEntry implementation:
  Type type() const override;
  const BloomQuestionAndAnswerEntry* AsQuestionAndAnswer() const override;

  BloomTextElement question;
  BloomTextElement answer;
  BloomSourceElement source;
};

struct COMPONENT_EXPORT(BLOOM) BloomExplainerEntry : public BloomResultEntry {
  BloomExplainerEntry();
  ~BloomExplainerEntry() override;

  // BloomResultEntry implementation:
  Type type() const override;
  const BloomExplainerEntry* AsExplainer() const override;

  BloomTitleElement title;
  BloomImageElement image;
  // An explainer ends with a variable amount of elements.
  std::vector<std::unique_ptr<BloomElement>> elements;
};

struct COMPONENT_EXPORT(BLOOM) BloomVideoEntry : public BloomResultEntry {
  BloomVideoEntry();
  ~BloomVideoEntry() override;

  // BloomResultEntry implementation:
  Type type() const override;
  const BloomVideoEntry* AsVideo() const override;

  BloomVideoElement video;
  BloomSourceElement source;
};

struct COMPONENT_EXPORT(BLOOM) BloomWebResultEntry : public BloomResultEntry {
  BloomWebResultEntry();
  ~BloomWebResultEntry() override;

  // BloomResultEntry implementation:
  Type type() const override;
  const BloomWebResultEntry* AsWebResult() const override;

  BloomTextElement title;
  BloomTextElement snippet;
  BloomSourceElement source;
};

struct COMPONENT_EXPORT(BLOOM) BloomResultSection {
  BloomResultSection();
  BloomResultSection(const BloomResultSection&) = delete;
  BloomResultSection& operator=(const BloomResultSection&) = delete;
  BloomResultSection(BloomResultSection&&);
  BloomResultSection& operator=(BloomResultSection&&);
  ~BloomResultSection();

  std::string title;
  std::vector<std::unique_ptr<BloomResultEntry>> entries;
};

struct COMPONENT_EXPORT(BLOOM) BloomResult {
 public:
  BloomResult();
  BloomResult(const BloomResult&) = delete;
  BloomResult& operator=(const BloomResult&) = delete;
  BloomResult(BloomResult&&);
  BloomResult& operator=(BloomResult&&);
  ~BloomResult();

  std::string query;
  std::vector<BloomResultSection> sections;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_RESULT_H_
