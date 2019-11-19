// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/page_features.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace dom_distiller {
/* This code needs to derive features in the same way and order in which they
 * are derived when training the model. Parts of that code are reproduced in the
 * comments below.
 */

namespace {

std::string GetLastSegment(const std::string& path) {
  // return re.search('[^/]*\/?$', path).group(0)
  if (path.size() == 0)
    return "";
  if (path.size() == 1) {
    DCHECK(path[0] == '/');
    return path;
  }
  size_t start = path.rfind("/", path.size() - 2);
  return start == std::string::npos ? "" : path.substr(start + 1);
}

int CountMatches(const std::string& s, const std::string& p) {
  // return len(re.findall(p, s))
  re2::StringPiece sp(s);
  re2::RE2 regexp(p);
  int count = 0;
  while (re2::RE2::FindAndConsume(&sp, regexp))
    count++;
  return count;
}

int GetWordCount(const std::string& s) {
  return CountMatches(s, "\\w+");
}

bool Contains(const std::string& n, const std::string& h) {
  return h.find(n) != std::string::npos;
}

bool EndsWith(const std::string& t, const std::string& s) {
  return s.size() >= t.size() &&
         s.compare(s.size() - t.size(), std::string::npos, t) == 0;
}

}  // namespace

int kDerivedFeaturesCount = 29;

std::vector<double> CalculateDerivedFeatures(bool isOGArticle,
                                             const GURL& url,
                                             double numElements,
                                             double numAnchors,
                                             double numForms,
                                             const std::string& innerText,
                                             const std::string& textContent,
                                             const std::string& innerHTML) {
  // In the training pipeline, the strings are explicitly encoded in utf-8 (as
  // they are here).
  const std::string& path = url.path();
  int innerTextWords = GetWordCount(innerText);
  int textContentWords = GetWordCount(textContent);
  int innerHTMLWords = GetWordCount(innerHTML);
  std::vector<double> features;
  // 'opengraph', opengraph,
  features.push_back(isOGArticle);
  // 'forum', 'forum' in path,
  features.push_back(Contains("forum", path));
  // 'index', 'index' in path,
  features.push_back(Contains("index", path));
  // 'view', 'view' in path,
  features.push_back(Contains("view", path));
  // 'asp', '.asp' in path,
  features.push_back(Contains(".asp", path));
  // 'phpbb', 'phpbb' in path,
  features.push_back(Contains("phpbb", path));
  // 'php', path.endswith('.php'),
  features.push_back(EndsWith(".php", path));
  // 'pathlength', len(path),
  features.push_back(path.size());
  // 'domain', len(path) < 2,
  features.push_back(path.size() < 2);
  // 'pathcomponents', CountMatches(path, r'\/.'),
  features.push_back(CountMatches(path, "\\/."));
  // 'slugdetector', CountMatches(path, r'[^\w/]'),
  features.push_back(CountMatches(path, "[^\\w/]"));
  // 'pathnumbers', CountMatches(path, r'\d+'),
  features.push_back(CountMatches(path, "\\d+"));
  // 'lastSegmentLength', len(GetLastSegment(path)),
  features.push_back(GetLastSegment(path).size());
  // 'formcount', numForms,
  features.push_back(numForms);
  // 'anchorcount', numAnchors,
  features.push_back(numAnchors);
  // 'elementcount', numElements,
  features.push_back(numElements);
  // 'anchorratio', float(numAnchors) / max(1, numElements),
  features.push_back(double(numAnchors) / std::max<double>(1, numElements));
  // 'innertextlength', len(innerText),
  features.push_back(innerText.size());
  // 'textcontentlength', len(textContent),
  features.push_back(textContent.size());
  // 'innerhtmllength', len(innerHTML),
  features.push_back(innerHTML.size());
  // 'innertextlengthratio', float(len(innerText)) / max(1, len(innerHTML)),
  features.push_back(double(innerText.size()) /
                     std::max<double>(1.0, innerHTML.size()));
  // 'textcontentlengthratio', float(len(textContent)) / max(1, len(innerHTML)),
  features.push_back(double(textContent.size()) /
                     std::max<double>(1.0, innerHTML.size()));
  // 'innertexttextcontentlengthratio',
  // float(len(innerText)) / max(1, len(textContent)),
  features.push_back(double(innerText.size()) /
                     std::max<double>(1.0, textContent.size()));
  // 'innertextwordcount', innerTextWords,
  features.push_back(innerTextWords);
  // 'textcontentwordcount', textContentWords,
  features.push_back(textContentWords);
  // 'innerhtmlwordcount', innerHTMLWords,
  features.push_back(innerHTMLWords);
  // 'innertextwordcountratio', float(innerTextWords) / max(1, innerHTMLWords),
  features.push_back(double(innerTextWords) /
                     std::max<int>(1.0, innerHTMLWords));
  // 'textcontentwordcountratio',
  // float(textContentWords) / max(1, innerHTMLWords),
  features.push_back(double(textContentWords) /
                     std::max<int>(1.0, innerHTMLWords));
  // 'innertexttextcontentwordcountratio',
  // float(innerTextWords) / max(1, textContentWords),
  features.push_back(double(innerTextWords) /
                     std::max<int>(1.0, textContentWords));
  return features;
}

std::vector<double> CalculateDerivedFeaturesFromJSON(
    const base::Value* stringified_json) {
  std::string stringified;
  if (!stringified_json->GetAsString(&stringified)) {
    return std::vector<double>();
  }

  std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(stringified);
  if (!json) {
    return std::vector<double>();
  }

  const base::DictionaryValue* dict;
  if (!json->GetAsDictionary(&dict)) {
    return std::vector<double>();
  }

  bool isOGArticle = false;
  std::string url, innerText, textContent, innerHTML;
  double numElements = 0.0, numAnchors = 0.0, numForms = 0.0;

  if (!(dict->GetBoolean("opengraph", &isOGArticle) &&
        dict->GetString("url", &url) &&
        dict->GetDouble("numElements", &numElements) &&
        dict->GetDouble("numAnchors", &numAnchors) &&
        dict->GetDouble("numForms", &numForms) &&
        dict->GetString("innerText", &innerText) &&
        dict->GetString("textContent", &textContent) &&
        dict->GetString("innerHTML", &innerHTML))) {
    return std::vector<double>();
  }

  GURL parsed_url(url);
  if (!parsed_url.is_valid()) {
    return std::vector<double>();
  }

  return CalculateDerivedFeatures(isOGArticle, parsed_url, numElements,
                                  numAnchors, numForms, innerText, textContent,
                                  innerHTML);
}

std::vector<double> CalculateDerivedFeatures(bool openGraph,
                                             const GURL& url,
                                             unsigned elementCount,
                                             unsigned anchorCount,
                                             unsigned formCount,
                                             double mozScore,
                                             double mozScoreAllSqrt,
                                             double mozScoreAllLinear) {
  const std::string& path = url.path();
  std::vector<double> features;
  // 'opengraph', opengraph,
  features.push_back(openGraph);
  // 'forum', 'forum' in path,
  features.push_back(Contains("forum", path));
  // 'index', 'index' in path,
  features.push_back(Contains("index", path));
  // 'search', 'search' in path,
  features.push_back(Contains("search", path));
  // 'view', 'view' in path,
  features.push_back(Contains("view", path));
  // 'archive', 'archive' in path,
  features.push_back(Contains("archive", path));
  // 'asp', '.asp' in path,
  features.push_back(Contains(".asp", path));
  // 'phpbb', 'phpbb' in path,
  features.push_back(Contains("phpbb", path));
  // 'php', path.endswith('.php'),
  features.push_back(EndsWith(".php", path));
  // 'pathLength', len(path),
  features.push_back(path.size());
  // 'domain', len(path) < 2,
  features.push_back(path.size() < 2);
  // 'pathComponents', CountMatches(path, r'\/.'),
  features.push_back(CountMatches(path, "\\/."));
  // 'slugDetector', CountMatches(path, r'[^\w/]'),
  features.push_back(CountMatches(path, "[^\\w/]"));
  // 'pathNumbers', CountMatches(path, r'\d+'),
  features.push_back(CountMatches(path, "\\d+"));
  // 'lastSegmentLength', len(GetLastSegment(path)),
  features.push_back(GetLastSegment(path).size());
  // 'formCount', numForms,
  features.push_back(formCount);
  // 'anchorCount', numAnchors,
  features.push_back(anchorCount);
  // 'elementCount', numElements,
  features.push_back(elementCount);
  // 'anchorRatio', float(numAnchors) / max(1, numElements),
  features.push_back(double(anchorCount) / std::max<double>(1, elementCount));
  // 'mozScore'
  features.push_back(mozScore);
  // 'mozScoreAllSqrt'
  features.push_back(mozScoreAllSqrt);
  // 'mozScoreAllLinear'
  features.push_back(mozScoreAllLinear);

  return features;
}

}  // namespace dom_distiller
