// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Integration with macOS built-in spellchecker.

#include "components/spellcheck/browser/spellcheck_platform.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/browser/browser_thread.h"

class PlatformSpellChecker;

using base::TimeTicks;
using content::BrowserThread;

namespace {
// The number of characters in the first part of the language code.
const unsigned int kShortLanguageCodeSize = 2;

// +[NSSpellChecker sharedSpellChecker] can throw exceptions depending
// on the state of the pasteboard, or possibly as a result of
// third-party code (when setting up services entries).  The following
// receives nil if an exception is thrown, in which case
// spell-checking will not work, but it also will not crash the
// browser.
NSSpellChecker* SharedSpellChecker() {
  @try {
    return NSSpellChecker.sharedSpellChecker;
  } @catch (id exception) {
    return nil;
  }
}

// A private utility function to convert hunspell language codes to macOS
// language codes.
NSString* ConvertLanguageCodeToMac(const std::string& hunspell_lang_code) {
  NSString* whole_code = base::SysUTF8ToNSString(hunspell_lang_code);

  if (whole_code.length > kShortLanguageCodeSize) {
    NSString* lang_code = [whole_code
                           substringToIndex:kShortLanguageCodeSize];
    // Add 1 here to skip the underscore.
    NSString* region_code = [whole_code
                             substringFromIndex:(kShortLanguageCodeSize + 1)];

    // Check for the special case of en-US and pt-PT, since macOS lists these
    // as just en and pt respectively.
    // TODO(pwicks): Find out if there are other special cases for languages
    // not installed on the system by default. Are there others like pt-PT?
    if (([lang_code isEqualToString:@"en"] &&
       [region_code isEqualToString:@"US"]) ||
        ([lang_code isEqualToString:@"pt"] &&
       [region_code isEqualToString:@"PT"])) {
      return lang_code;
    }

    // Otherwise, just build a string that uses an underscore instead of a
    // dash between the language and the region code, since this is the
    // format that macOS uses.
    NSString* macos_language =
        [NSString stringWithFormat:@"%@_%@", lang_code, region_code];
    return macos_language;
  } else {
    // Special case for Polish.
    if ([whole_code isEqualToString:@"pl"]) {
      return @"pl_PL";
    }
    // This is just a language code with the same format as macOS
    // language code.
    return whole_code;
  }
}

std::string ConvertLanguageCodeFromMac(NSString* lang_code) {
  // TODO(pwicks):figure out what to do about Multilingual
  // Guards for strange cases.
  if ([lang_code isEqualToString:@"en"]) {
    return std::string("en-US");
  }
  if ([lang_code isEqualToString:@"pt"]) {
    return std::string("pt-PT");
  }
  if ([lang_code isEqualToString:@"pl_PL"]) {
    return std::string("pl");
  }

  if (lang_code.length > kShortLanguageCodeSize &&
      [lang_code characterAtIndex:kShortLanguageCodeSize] == '_') {
    return base::SysNSStringToUTF8([NSString
        stringWithFormat:@"%@-%@",
                         [lang_code substringToIndex:kShortLanguageCodeSize],
                         [lang_code
                             substringFromIndex:(kShortLanguageCodeSize + 1)]]);
  }
  return base::SysNSStringToUTF8(lang_code);
}

} // namespace

namespace spellcheck_platform {

void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages) {
  for (NSString* lang_code in SharedSpellChecker().availableLanguages) {
    spellcheck_languages->push_back(
              ConvertLanguageCodeFromMac(lang_code));
  }
}

void RetrieveSpellcheckLanguages(
    PlatformSpellChecker* spell_checker_instance,
    RetrieveSpellcheckLanguagesCompleteCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(std::vector<std::string>());
}

void AddSpellcheckLanguagesForTesting(
    PlatformSpellChecker* spell_checker_instance,
    const std::vector<std::string>& languages) {
  NOTIMPLEMENTED();
}

std::string GetSpellCheckerLanguage() {
  return ConvertLanguageCodeFromMac(SharedSpellChecker().language);
}

bool SpellCheckerAvailable() {
  return true;
}

bool SpellCheckerProvidesPanel() {
  // macOS has a Spelling Panel, so we can return true here.
  return true;
}

bool SpellingPanelVisible() {
  // This should only be called from the main thread.
  DCHECK(NSThread.isMainThread);
  return SharedSpellChecker().spellingPanel.visible;
}

void ShowSpellingPanel(bool show) {
  if (show) {
    [SharedSpellChecker().spellingPanel
        performSelectorOnMainThread:@selector(makeKeyAndOrderFront:)
                         withObject:nil
                      waitUntilDone:YES];
  } else {
    [SharedSpellChecker().spellingPanel
        performSelectorOnMainThread:@selector(close)
                         withObject:nil
                      waitUntilDone:YES];
  }
}

void UpdateSpellingPanelWithMisspelledWord(const std::u16string& word) {
  NSString * word_to_display = base::SysUTF16ToNSString(word);
  [SharedSpellChecker()
      performSelectorOnMainThread:
        @selector(updateSpellingPanelWithMisspelledWord:)
                       withObject:word_to_display
                    waitUntilDone:YES];
}

void PlatformSupportsLanguage(PlatformSpellChecker* spell_checker_instance,
                              const std::string& current_language,
                              base::OnceCallback<void(bool)> callback) {
  // First, convert the language to an macOS language code.
  NSString* mac_lang_code = ConvertLanguageCodeToMac(current_language);

  // Then grab the languages available.
  NSArray* availableLanguages = SharedSpellChecker().availableLanguages;

  // Return true if the given language is supported by macOS.
  std::move(callback).Run([availableLanguages containsObject:mac_lang_code]);
}

void SetLanguage(PlatformSpellChecker* spell_checker_instance,
                 const std::string& lang_to_set,
                 base::OnceCallback<void(bool)> callback) {
  // Do not set any language right now, since Chrome should honor the
  // system spellcheck settings. (http://crbug.com/166046)
  // Fix this once Chrome actually allows setting a spellcheck language
  // in chrome://settings.
  //  NSString* NS_lang_to_set = ConvertLanguageCodeToMac(lang_to_set);
  //  [SharedSpellChecker() setLanguage:NS_lang_to_set];
  std::move(callback).Run(true);
}

void DisableLanguage(PlatformSpellChecker* spell_checker_instance,
                     const std::string& lang_to_disable) {}

static int last_seen_tag_;

bool CheckSpelling(const std::u16string& word_to_check, int tag) {
  last_seen_tag_ = tag;

  // -[NSSpellChecker checkSpellingOfString] returns an NSRange that
  // we can look at to determine if a word is misspelled.
  NSRange spell_range = {0,0};

  // Convert the word to an NSString.
  NSString* NS_word_to_check = base::SysUTF16ToNSString(word_to_check);
  // Check the spelling, starting at the beginning of the word.
  spell_range = [SharedSpellChecker() checkSpellingOfString:NS_word_to_check
                                                 startingAt:0
                                                   language:nil
                                                       wrap:NO
                                     inSpellDocumentWithTag:tag
                                                  wordCount:nullptr];

  // If the length of the misspelled word == 0,
  // then there is no misspelled word.
  bool word_correct = (spell_range.length == 0);
  return word_correct;
}

void FillSuggestionList(const std::u16string& wrong_word,
                        std::vector<std::u16string>* optional_suggestions) {
  NSString* ns_wrong_word = base::SysUTF16ToNSString(wrong_word);
  NSSpellChecker* checker = SharedSpellChecker();
  NSString* language = [checker language];
  NSArray* guesses =
      [checker guessesForWordRange:NSMakeRange(0, ns_wrong_word.length)
                          inString:ns_wrong_word
                          language:language
            inSpellDocumentWithTag:last_seen_tag_];
  int i = 0;
  for (NSString* guess in guesses) {
    optional_suggestions->push_back(base::SysNSStringToUTF16(guess));
    if (++i >= spellcheck::kMaxSuggestions)
      break;
  }
}

void AddWord(PlatformSpellChecker* spell_checker_instance,
             const std::u16string& word) {
  NSString* word_to_add = base::SysUTF16ToNSString(word);
  [SharedSpellChecker() learnWord:word_to_add];
}

void RemoveWord(PlatformSpellChecker* spell_checker_instance,
                const std::u16string& word) {
  NSString *word_to_remove = base::SysUTF16ToNSString(word);
  [SharedSpellChecker() unlearnWord:word_to_remove];
}

int GetDocumentTag() {
  NSInteger doc_tag = [NSSpellChecker uniqueSpellDocumentTag];
  return static_cast<int>(doc_tag);
}

void IgnoreWord(PlatformSpellChecker* spell_checker_instance,
                const std::u16string& word) {
  [SharedSpellChecker() ignoreWord:base::SysUTF16ToNSString(word)
            inSpellDocumentWithTag:last_seen_tag_];
}

void CloseDocumentWithTag(int tag) {
  [SharedSpellChecker() closeSpellDocumentWithTag:static_cast<NSInteger>(tag)];
}

void RequestTextCheck(PlatformSpellChecker* spell_checker_instance,
                      int document_tag,
                      const std::u16string& text,
                      TextCheckCompleteCallback passed_callback) {
  NSString* text_to_check = base::SysUTF16ToNSString(text);
  NSRange range_to_check = NSMakeRange(0, text_to_check.length);
  __block TextCheckCompleteCallback callback(std::move(passed_callback));

  [SharedSpellChecker()
      requestCheckingOfString:text_to_check
                        range:range_to_check
                        types:NSTextCheckingTypeSpelling
                      options:nil
       inSpellDocumentWithTag:document_tag
            completionHandler:^(NSInteger,
                                NSArray *results,
                                NSOrthography*,
                                NSInteger) {
          std::vector<SpellCheckResult> check_results;
          for (NSTextCheckingResult* result in results) {
            // Deliberately ignore non-spelling results. macOS at the very least
            // delivers a result of NSTextCheckingTypeOrthography for the
            // whole fragment, which underlines the entire checked range.
            if (result.resultType != NSTextCheckingTypeSpelling) {
              continue;
            }

            // In this use case, the spell checker should never
            // return anything but a single range per result.
            check_results.emplace_back(SpellCheckResult::SPELLING,
                                       result.range.location,
                                       result.range.length);
          }
          // TODO(groby): Verify we don't need to post from here.
          std::move(callback).Run(check_results);
      }];
}

class SpellcheckerStateInternal {
 public:
  SpellcheckerStateInternal();
  ~SpellcheckerStateInternal();

 private:
  BOOL automaticallyIdentifiesLanguages_;
  NSString* language_;
};

SpellcheckerStateInternal::SpellcheckerStateInternal() {
  language_ = SharedSpellChecker().language;
  automaticallyIdentifiesLanguages_ =
      SharedSpellChecker().automaticallyIdentifiesLanguages;
  [SharedSpellChecker() setLanguage:@"en"];
  SharedSpellChecker().automaticallyIdentifiesLanguages = NO;
}

SpellcheckerStateInternal::~SpellcheckerStateInternal() {
  [SharedSpellChecker() setLanguage:language_];
  SharedSpellChecker().automaticallyIdentifiesLanguages =
      automaticallyIdentifiesLanguages_;
}

ScopedEnglishLanguageForTest::ScopedEnglishLanguageForTest()
    : state_(new SpellcheckerStateInternal) {
}

ScopedEnglishLanguageForTest::~ScopedEnglishLanguageForTest() {
  delete state_;
}

}  // namespace spellcheck_platform
