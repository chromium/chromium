// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_parser.h"

#include <string.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

// Defines for element names of the OSD document:
const char kURLElement[] = "Url";
const char kParamElement[] = "Param";
const char kShortNameElement[] = "ShortName";
const char kImageElement[] = "Image";
const char kOpenSearchDescriptionElement[] = "OpenSearchDescription";
const char kFirefoxSearchDescriptionElement[] = "SearchPlugin";
const char kInputEncodingElement[] = "InputEncoding";
const char kAliasElement[] = "Alias";

// Various XML attributes used.
const char kURLTypeAttribute[] = "type";
const char kURLTemplateAttribute[] = "template";
const char kImageTypeAttribute[] = "type";
const char kImageWidthAttribute[] = "width";
const char kImageHeightAttribute[] = "height";
const char kParamNameAttribute[] = "name";
const char kParamValueAttribute[] = "value";
const char kParamMethodAttribute[] = "method";

// Mime type for search results.
const char kHTMLType[] = "text/html";

// Mime type for as you type suggestions.
const char kSuggestionType[] = "application/x-suggestions+json";

// Returns true if input_encoding contains a valid input encoding string. This
// doesn't verify that we have a valid encoding for the string, just that the
// string contains characters that constitute a valid input encoding.
bool IsValidEncodingString(const std::string& input_encoding) {
  if (input_encoding.empty())
    return false;

  if (!base::IsAsciiAlpha(input_encoding[0]))
    return false;

  for (size_t i = 1, max = input_encoding.size(); i < max; ++i) {
    char c = input_encoding[i];
    if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) &&
        c != '.' && c != '_' && c != '-') {
      return false;
    }
  }
  return true;
}

void AppendParamToQuery(const std::string& key,
                        const std::string& value,
                        std::string* query) {
  if (!query->empty())
    query->append("&");
  if (!key.empty()) {
    query->append(key);
    query->append("=");
  }
  query->append(value);
}

// Returns true if |url| is empty or is a valid URL with a scheme of HTTP[S].
bool IsHTTPRef(const std::string& url) {
  if (url.empty())
    return true;
  GURL gurl(url);
  return gurl.is_valid() && (gurl.SchemeIs(url::kHttpScheme) ||
                             gurl.SchemeIs(url::kHttpsScheme));
}

// SafeTemplateURLParser takes the output of the data_decoder service's
// XmlParser and extracts the data from the search description into a
// TemplateURL.
class SafeTemplateURLParser {
 public:
  enum Method {
    GET,
    POST
  };

  // Key/value of a Param node.
  using Param = std::pair<std::string, std::string>;

  SafeTemplateURLParser(
      const SearchTermsData* search_terms_data,
      const TemplateURLParser::ParameterFilter& parameter_filter,
      TemplateURLParser::ParseCallback callback)
      : search_terms_data_(SearchTermsData::MakeSnapshot(search_terms_data)),
        parameter_filter_(parameter_filter),
        callback_(std::move(callback)) {}

  SafeTemplateURLParser(const SafeTemplateURLParser&) = delete;
  SafeTemplateURLParser& operator=(const SafeTemplateURLParser&) = delete;

  // Parse callback for DataDecoder::ParseXml(). This calls the callback
  // passed to the constructor upon completion.
  void OnXmlParseComplete(
      data_decoder::DataDecoder::ValueOrError value_or_error);

 private:
  void ParseURLs(const std::vector<const base::Value*>& urls);
  void ParseImages(const std::vector<const base::Value*>& images);
  void ParseEncodings(const std::vector<const base::Value*>& encodings);
  void ParseAliases(const std::vector<const base::Value*>& aliases);

  std::unique_ptr<TemplateURL> FinalizeTemplateURL();

  // Returns all child elements of |elem| named |tag|, which are searched
  // for using the XML qualified namespaces in |namespaces_|.
  bool GetChildElementsByTag(const base::Value& elem,
                             const std::string& tag,
                             std::vector<const base::Value*>* children);

  // Data that gets updated as we parse, and is converted to a TemplateURL by
  // FinalizeTemplateURL().
  TemplateURLData data_;

  // The HTTP methods used.
  Method method_ = GET;
  Method suggestion_method_ = GET;

  // If true, the user has set a keyword and we should use it. Otherwise,
  // we generate a keyword based on the URL.
  bool has_custom_keyword_ = false;

  // Whether we should derive the image from the URL (when images are data
  // URLs).
  bool derive_image_from_url_ = false;

  // The XML namespaces that were declared on the root element. These are used
  // to search for tags by name in GetChildElementsByTag(). Will always contain
  // at least one element, if only the empty string.
  std::vector<std::string> namespaces_;

  // We have to own our own snapshot, because the parse request may outlive the
  // originally provided SearchTermsData lifetime.
  std::unique_ptr<SearchTermsData> search_terms_data_;

  TemplateURLParser::ParameterFilter parameter_filter_;
  TemplateURLParser::ParseCallback callback_;
};

void SafeTemplateURLParser::OnXmlParseComplete(
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  std::move(callback_).Run([&]() -> std::unique_ptr<TemplateURL> {
    ASSIGN_OR_RETURN(const base::Value root, std::move(value_or_error),
                     [](std::string error) -> std::unique_ptr<TemplateURL> {
                       DLOG(ERROR)
                           << "Failed to parse XML: " << std::move(error);
                       return nullptr;
                     });

    // Get the namespaces used in the XML document, which will be used
    // to access nodes by tag name in GetChildElementsByTag().
    if (const base::Value::Dict* namespaces = root.GetDict().FindDict(
            data_decoder::mojom::XmlParser::kNamespacesKey)) {
      for (auto item : *namespaces) {
        namespaces_.push_back(item.first);
      }
    }
    if (namespaces_.empty()) {
      namespaces_.emplace_back();
    }

    std::string root_tag;
    if (!data_decoder::GetXmlElementTagName(root, &root_tag) ||
        (root_tag != kOpenSearchDescriptionElement &&
         root_tag != kFirefoxSearchDescriptionElement)) {
      DLOG(ERROR) << "Unexpected root tag: " << root_tag;
      return nullptr;
    }

    // The only required element is the URL.
    std::vector<const base::Value*> urls;
    if (!GetChildElementsByTag(root, kURLElement, &urls)) {
      return nullptr;
    }
    ParseURLs(urls);

    std::vector<const base::Value*> images;
    if (GetChildElementsByTag(root, kImageElement, &images)) {
      ParseImages(images);
    }

    std::vector<const base::Value*> encodings;
    if (GetChildElementsByTag(root, kInputEncodingElement, &encodings)) {
      ParseEncodings(encodings);
    }

    std::vector<const base::Value*> aliases;
    if (GetChildElementsByTag(root, kAliasElement, &aliases)) {
      ParseAliases(aliases);
    }

    std::vector<const base::Value*> short_names;
    if (GetChildElementsByTag(root, kShortNameElement, &short_names)) {
      std::string name;
      if (data_decoder::GetXmlElementText(*short_names.back(), &name)) {
        data_.SetShortName(base::UTF8ToUTF16(name));
      }
    }

    return FinalizeTemplateURL();
  }());
}

void SafeTemplateURLParser::ParseURLs(
    const std::vector<const base::Value*>& urls) {
  for (auto* url_value : urls) {
    std::string template_url =
        data_decoder::GetXmlElementAttribute(*url_value, kURLTemplateAttribute);
    std::string type =
        data_decoder::GetXmlElementAttribute(*url_value, kURLTypeAttribute);
    bool is_post = base::EqualsCaseInsensitiveASCII(
        data_decoder::GetXmlElementAttribute(*url_value, kParamMethodAttribute),
        "post");
    bool is_html_url = (type == kHTMLType);
    bool is_suggest_url = (type == kSuggestionType);

    if (is_html_url && !template_url.empty()) {
      data_.SetURL(template_url);
      is_suggest_url = false;
      if (is_post)
        method_ = POST;
    } else if (is_suggest_url) {
      data_.suggestions_url = template_url;
      if (is_post)
        suggestion_method_ = POST;
    }

    std::vector<Param> extra_params;

    std::vector<const base::Value*> params;
    GetChildElementsByTag(*url_value, kParamElement, &params);
    for (auto* param : params) {
      std::string key =
          data_decoder::GetXmlElementAttribute(*param, kParamNameAttribute);
      std::string value =
          data_decoder::GetXmlElementAttribute(*param, kParamValueAttribute);
      if (!key.empty() &&
          (parameter_filter_.is_null() || parameter_filter_.Run(key, value))) {
        extra_params.push_back(Param(key, value));
      }
    }

    if (!parameter_filter_.is_null() || !extra_params.empty()) {
      GURL url(is_suggest_url ? data_.suggestions_url : data_.url());
      if (!url.is_valid())
        return;

      // If there is a parameter filter, parse the existing URL and remove any
      // unwanted parameter.
      std::string new_query;
      bool modified = false;
      if (!parameter_filter_.is_null()) {
        url::Component query = url.parsed_for_possibly_invalid_spec().query;
        url::Component key, value;
        const char* url_spec = url.spec().c_str();
        while (url::ExtractQueryKeyValue(url_spec, &query, &key, &value)) {
          std::string key_str(url_spec, key.begin, key.len);
          std::string value_str(url_spec, value.begin, value.len);
          if (parameter_filter_.Run(key_str, value_str)) {
            AppendParamToQuery(key_str, value_str, &new_query);
          } else {
            modified = true;
          }
        }
      }
      if (!modified)
        new_query = url.query();

      // Add the extra parameters if any.
      if (!extra_params.empty()) {
        modified = true;
        for (const auto& iter : extra_params)
          AppendParamToQuery(iter.first, iter.second, &new_query);
      }

      if (modified) {
        GURL::Replacements repl;
        repl.SetQueryStr(new_query);
        url = url.ReplaceComponents(repl);
        if (is_suggest_url)
          data_.suggestions_url = url.spec();
        else if (url.is_valid())
          data_.SetURL(url.spec());
      }
    }
  }
}

void SafeTemplateURLParser::ParseImages(
    const std::vector<const base::Value*>& images) {
  for (auto* image : images) {
    std::string url_string;
    if (!data_decoder::GetXmlElementText(*image, &url_string))
      continue;

    std::string type =
        data_decoder::GetXmlElementAttribute(*image, kImageTypeAttribute);
    int width = 0;
    int height = 0;
    base::StringToInt(
        data_decoder::GetXmlElementAttribute(*image, kImageWidthAttribute),
        &width);
    base::StringToInt(
        data_decoder::GetXmlElementAttribute(*image, kImageHeightAttribute),
        &height);

    bool image_is_valid_for_favicon =
        (width == gfx::kFaviconSize) && (height == gfx::kFaviconSize) &&
        ((type == "image/x-icon") || (type == "image/vnd.microsoft.icon"));

    GURL image_url(url_string);

    if (image_url.SchemeIs(url::kDataScheme)) {
      // TODO(jcampan): bug 1169256: when dealing with data URL, we need to
      // decode the data URL in the renderer. For now, we'll just point to the
      // favicon from the URL.
      derive_image_from_url_ = true;
    } else if (image_is_valid_for_favicon && image_url.is_valid() &&
               (image_url.SchemeIs(url::kHttpScheme) ||
                image_url.SchemeIs(url::kHttpsScheme))) {
      data_.favicon_url = image_url;
    }
    image_is_valid_for_favicon = false;
  }
}

void SafeTemplateURLParser::ParseEncodings(
    const std::vector<const base::Value*>& encodings) {
  for (auto* encoding : encodings) {
    std::string encoding_value;
    if (data_decoder::GetXmlElementText(*encoding, &encoding_value)) {
      if (IsValidEncodingString(encoding_value))
        data_.input_encodings.push_back(encoding_value);
    }
  }
}

void SafeTemplateURLParser::ParseAliases(
    const std::vector<const base::Value*>& aliases) {
  for (auto* alias : aliases) {
    std::string alias_value;
    if (data_decoder::GetXmlElementText(*alias, &alias_value)) {
      data_.SetKeyword(base::UTF8ToUTF16(alias_value));
      has_custom_keyword_ = true;
    }
  }
}

std::unique_ptr<TemplateURL> SafeTemplateURLParser::FinalizeTemplateURL() {
  // TODO(crbug.com/40304654): Support engines that use POST.
  if (method_ == POST || !IsHTTPRef(data_.url()) ||
      !IsHTTPRef(data_.suggestions_url)) {
    DLOG(ERROR) << "POST URLs are not supported";
    return nullptr;
  }
  if (suggestion_method_ == POST)
    data_.suggestions_url.clear();

  // If the image was a data URL, use the favicon from the search URL instead.
  // (see the TODO in ParseImages()).
  GURL search_url(data_.url());
  if (derive_image_from_url_ && data_.favicon_url.is_empty())
    data_.favicon_url = TemplateURL::GenerateFaviconURL(search_url);

  // Generate a keyword for this search engine if a custom one was not present
  // in the imported data.
  if (!has_custom_keyword_)
    data_.SetKeyword(TemplateURL::GenerateKeyword(search_url));

  // If the OSDD omits or has an empty short name, use the keyword.
  if (data_.short_name().empty())
    data_.SetShortName(data_.keyword());

  // Bail if the search URL is empty or if either TemplateURLRef is invalid.
  std::unique_ptr<TemplateURL> template_url =
      std::make_unique<TemplateURL>(data_);
  if (template_url->url().empty() ||
      !template_url->url_ref().IsValid(*search_terms_data_) ||
      (!template_url->suggestions_url().empty() &&
       !template_url->suggestions_url_ref().IsValid(*search_terms_data_))) {
    DLOG(ERROR) << "Template URL is not valid";
    return nullptr;
  }

  return template_url;
}

bool SafeTemplateURLParser::GetChildElementsByTag(
    const base::Value& elem,
    const std::string& tag,
    std::vector<const base::Value*>* children) {
  bool result = false;
  for (const auto& ns : namespaces_) {
    std::string name = data_decoder::GetXmlQualifiedName(ns, tag);
    result |=
        data_decoder::GetAllXmlElementChildrenWithTag(elem, name, children);
  }
  return result;
}

}  // namespace

// TemplateURLParser ----------------------------------------------------------

// static
void TemplateURLParser::Parse(const SearchTermsData* search_terms_data,
                              const std::string& data,
                              const ParameterFilter& parameter_filter,
                              ParseCallback completion_callback) {
  auto safe_parser = std::make_unique<SafeTemplateURLParser>(
      search_terms_data, parameter_filter, std::move(completion_callback));
  data_decoder::DataDecoder::ParseXmlIsolated(
      data, data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
      base::BindOnce(&SafeTemplateURLParser::OnXmlParseComplete,
                     std::move(safe_parser)));
}

// static
void TemplateURLParser::ParseWithDataDecoder(
    data_decoder::DataDecoder* data_decoder,
    const SearchTermsData* search_terms_data,
    const std::string& data,
    const ParameterFilter& parameter_filter,
    ParseCallback completion_callback) {
  auto safe_parser = std::make_unique<SafeTemplateURLParser>(
      search_terms_data, parameter_filter, std::move(completion_callback));
  data_decoder->ParseXml(
      data, data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
      base::BindOnce(&SafeTemplateURLParser::OnXmlParseComplete,
                     std::move(safe_parser)));
}
