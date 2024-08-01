// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/spellcheck/browser/spellcheck_platform.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SpellcheckPlatformMacTest: public testing::Test {
 public:
  SpellcheckPlatformMacTest()
      : callback_(base::BindOnce(&SpellcheckPlatformMacTest::CompletionCallback,
                                 base::Unretained(this))),
        callback_finished_(false) {}

  void WaitForCallback() { loop_.Run(); }

  std::vector<SpellCheckResult> results_;
  spellcheck_platform::TextCheckCompleteCallback callback_;
  bool callback_finished_;

 private:
  void QuitMessageLoop() {
    ASSERT_TRUE(
        task_environment_.GetMainThreadTaskRunner()->BelongsToCurrentThread());
    loop_.Quit();
  }

  void CompletionCallback(const std::vector<SpellCheckResult>& results) {
    results_ = results;
    callback_finished_ = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&SpellcheckPlatformMacTest::QuitMessageLoop,
                                  base::Unretained(this)));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  spellcheck_platform::ScopedEnglishLanguageForTest scoped_language_;
  base::RunLoop loop_;
};

// Tests that words are properly ignored. Currently only enabled on OS X as it
// is the only platform to support ignoring words. Note that in this test, we
// supply a non-zero doc_tag, in order to test that ignored words are matched to
// the correct document.
TEST_F(SpellcheckPlatformMacTest, IgnoreWords_EN_US) {
  const char* kTestCases[] = {
    "teh",
    "morblier",
    "watre",
    "noooen",
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const std::u16string word(base::ASCIIToUTF16(kTestCases[i]));
    const int doc_tag = spellcheck_platform::GetDocumentTag();

    // The word should show up as misspelled.
    EXPECT_FALSE(spellcheck_platform::CheckSpelling(word, doc_tag)) << word;

    // Ignore the word.
    spellcheck_platform::IgnoreWord(nullptr, word);

    // The word should now show up as correctly spelled.
    EXPECT_TRUE(spellcheck_platform::CheckSpelling(word, doc_tag)) << word;

    // Close the docuemnt. Any words that we had previously ignored should no
    // longer be ignored and thus should show up as misspelled.
    spellcheck_platform::CloseDocumentWithTag(doc_tag);

    // The word should now show be spelled wrong again
    EXPECT_FALSE(spellcheck_platform::CheckSpelling(word, doc_tag)) << word;
  }
}  // Test IgnoreWords_EN_US

TEST_F(SpellcheckPlatformMacTest, SpellCheckSuggestions_EN_US) {
  static const struct {
    const char* input;           // A string to be tested.
    const char* suggested_word;  // A suggested word that should occur.
  } kTestCases[] = {
    // We need to have separate test cases here, since hunspell and the OS X
    // spellchecking service occasionally differ on what they consider a valid
    // suggestion for a given word, although these lists could likely be
    // integrated somewhat. The test cases for non-Mac are in
    // chrome/renderer/spellcheck_unittest.cc
    // These words come from the wikipedia page of the most commonly
    // misspelled words in english.
    // (http://en.wikipedia.org/wiki/Commonly_misspelled_words).
    // However, 10.6 loads multiple dictionaries and enables many non-English
    // dictionaries by default. As a result, we have removed from the list any
    // word that is marked as correct because it is correct in another
    // language.
    {"absense", "absence"},
    {"acceptible", "acceptable"},
    {"accidentaly", "accidentally"},
    {"acheive", "achieve"},
    {"acknowlege", "acknowledge"},
    {"acquaintence", "acquaintance"},
    {"aquire", "acquire"},
    {"aquit", "acquit"},
    {"acrage", "acreage"},
    {"adultary", "adultery"},
    {"advertize", "advertise"},
    {"adviseable", "advisable"},
    {"alchohol", "alcohol"},
    {"alege", "allege"},
    {"allegaince", "allegiance"},
    {"allmost", "almost"},
    // Ideally, this test should pass. It works in firefox, but not in hunspell
    // or OS X.
    // {"alot", "a lot"},
    {"amatuer", "amateur"},
    {"ammend", "amend"},
    {"amung", "among"},
    {"anually", "annually"},
    {"apparant", "apparent"},
    {"artic", "arctic"},
    {"arguement", "argument"},
    {"athiest", "atheist"},
    {"athelete", "athlete"},
    {"avrage", "average"},
    {"awfull", "awful"},
    {"ballance", "balance"},
    {"basicly", "basically"},
    {"becuase", "because"},
    {"becomeing", "becoming"},
    {"befor", "before"},
    {"begining", "beginning"},
    {"beleive", "believe"},
    {"bellweather", "bellwether"},
    {"benifit", "benefit"},
    // This particular spelling correction was removed in OSX 10.10. Replacing
    // it with another spelling correction that also works on older OSes.
    // {"bouy", "buoy"},
    {"bouy", "body"},
    {"briliant", "brilliant"},
    {"burgler", "burglar"},
    {"camoflage", "camouflage"},
    {"carefull", "careful"},
    {"Carribean", "Caribbean"},
    {"catagory", "category"},
    {"cauhgt", "caught"},
    {"cieling", "ceiling"},
    {"cemetary", "cemetery"},
    {"certin", "certain"},
    {"changable", "changeable"},
    {"cheif", "chief"},
    {"citezen", "citizen"},
    {"collaegue", "colleague"},
    {"colum", "column"},
    {"comming", "coming"},
    {"commited", "committed"},
    {"compitition", "competition"},
    {"conceed", "concede"},
    {"congradulate", "congratulate"},
    {"consciencious", "conscientious"},
    {"concious", "conscious"},
    {"concensus", "consensus"},
    {"contraversy", "controversy"},
    {"conveniance", "convenience"},
    {"critecize", "criticize"},
    {"dacquiri", "daiquiri"},
    {"decieve", "deceive"},
    {"dicide", "decide"},
    {"definate", "definite"},
    {"definitly", "definitely"},
    {"desparate", "desperate"},
    {"develope", "develop"},
    {"diffrence", "difference"},
    {"disapear", "disappear"},
    {"disapoint", "disappoint"},
    {"disasterous", "disastrous"},
    {"disipline", "discipline"},
    {"drunkeness", "drunkenness"},
    {"dumbell", "dumbbell"},
    {"easely", "easily"},
    {"eigth", "eight"},
    {"embarass", "embarrass"},
    {"enviroment", "environment"},
    {"equiped", "equipped"},
    {"equiptment", "equipment"},
    {"exagerate", "exaggerate"},
    {"exellent", "excellent"},
    {"exsept", "except"},
    {"exercize", "exercise"},
    {"exilerate", "exhilarate"},
    {"existance", "existence"},
    {"experiance", "experience"},
    {"experament", "experiment"},
    {"explaination", "explanation"},
    {"facinating", "fascinating"},
    {"firey", "fiery"},
    {"finaly", "finally"},
    {"flourescent", "fluorescent"},
    {"foriegn", "foreign"},
    {"fourty", "forty"},
    {"foreward", "forward"},
    {"freind", "friend"},
    {"fundemental", "fundamental"},
    {"guage", "gauge"},
    {"generaly", "generally"},
    {"goverment", "government"},
    {"gratefull", "grateful"},
    {"garantee", "guarantee"},
    {"guidence", "guidance"},
    {"happyness", "happiness"},
    {"harrass", "harass"},
    {"heighth", "height"},
    {"heirarchy", "hierarchy"},
    {"humerous", "humorous"},
    {"hygene", "hygiene"},
    {"hipocrit", "hypocrite"},
    {"idenity", "identity"},
    {"ignorence", "ignorance"},
    {"imaginery", "imaginary"},
    {"immitate", "imitate"},
    {"immitation", "imitation"},
    {"imediately", "immediately"},
    {"incidently", "incidentally"},
    {"independant", "independent"},
    {"indispensible", "indispensable"},
    {"innoculate", "inoculate"},
    {"inteligence", "intelligence"},
    {"intresting", "interesting"},
    {"interuption", "interruption"},
    {"irrelevent", "irrelevant"},
    {"irritible", "irritable"},
    {"jellous", "jealous"},
    {"knowlege", "knowledge"},
    {"labratory", "laboratory"},
    {"lenght", "length"},
    {"liason", "liaison"},
    {"libary", "library"},
    {"lisence", "license"},
    {"lonelyness", "loneliness"},
    {"lieing", "lying"},
    {"maintenence", "maintenance"},
    {"manuever", "maneuver"},
    {"marrige", "marriage"},
    {"mathmatics", "mathematics"},
    {"medcine", "medicine"},
    {"miniture", "miniature"},
    {"minite", "minute"},
    {"mischevous", "mischievous"},
    {"mispell", "misspell"},
    // Maybe this one should pass, as it works in hunspell, but not in firefox.
    // {"misterius", "mysterious"},
    {"naturaly", "naturally"},
    {"neccessary", "necessary"},
    {"neice", "niece"},
    {"nieghbor", "neighbor"},
    {"nieghbour", "neighbor"},
    {"niether", "neither"},
    {"noticable", "noticeable"},
    {"occassion", "occasion"},
    {"occasionaly", "occasionally"},
    {"occurrance", "occurrence"},
    {"occured", "occurred"},
    {"ommision", "omission"},
    {"oppurtunity", "opportunity"},
    {"outragous", "outrageous"},
    {"parrallel", "parallel"},
    {"parliment", "parliament"},
    {"particurly", "particularly"},
    {"passtime", "pastime"},
    {"peculier", "peculiar"},
    {"percieve", "perceive"},
    {"pernament", "permanent"},
    {"perseverence", "perseverance"},
    {"personaly", "personally"},
    {"persaude", "persuade"},
    {"pichure", "picture"},
    {"peice", "piece"},
    {"plagerize", "plagiarize"},
    {"playright", "playwright"},
    {"plesant", "pleasant"},
    {"pollitical", "political"},
    {"posession", "possession"},
    {"potatos", "potatoes"},
    {"practicle", "practical"},
    {"preceed", "precede"},
    {"predjudice", "prejudice"},
    {"presance", "presence"},
    {"privelege", "privilege"},
    // This one should probably work. It does in FF and Hunspell.
    // {"probly", "probably"},
    {"proffesional", "professional"},
    {"promiss", "promise"},
    {"pronounciation", "pronunciation"},
    {"prufe", "proof"},
    {"psycology", "psychology"},
    {"publically", "publicly"},
    {"quanity", "quantity"},
    {"quarentine", "quarantine"},
    {"questionaire", "questionnaire"},
    {"readible", "readable"},
    {"realy", "really"},
    {"recieve", "receive"},
    {"reciept", "receipt"},
    {"reconize", "recognize"},
    {"recomend", "recommend"},
    {"refered", "referred"},
    {"referance", "reference"},
    {"relevent", "relevant"},
    {"religous", "religious"},
    {"repitition", "repetition"},
    {"restarant", "restaurant"},
    {"rythm", "rhythm"},
    {"rediculous", "ridiculous"},
    {"sacrefice", "sacrifice"},
    {"saftey", "safety"},
    {"sissors", "scissors"},
    {"secratary", "secretary"},
    {"seperate", "separate"},
    {"sargent", "sergeant"},
    {"shineing", "shining"},
    {"similer", "similar"},
    {"sinceerly", "sincerely"},
    {"speach", "speech"},
    {"strenght", "strength"},
    {"succesful", "successful"},
    {"supercede", "supersede"},
    {"surelly", "surely"},
    {"suprise", "surprise"},
    {"temperture", "temperature"},
    {"temprary", "temporary"},
    {"tommorrow", "tomorrow"},
    {"tounge", "tongue"},
    {"truely", "truly"},
    {"twelth", "twelfth"},
    {"tyrany", "tyranny"},
    {"underate", "underrate"},
    {"untill", "until"},
    {"unuseual", "unusual"},
    {"upholstry", "upholstery"},
    {"usible", "usable"},
    {"useing", "using"},
    {"usualy", "usually"},
    {"vaccuum", "vacuum"},
    {"vegatarian", "vegetarian"},
    {"vehical", "vehicle"},
    {"visious", "vicious"},
    {"villege", "village"},
    {"wierd", "weird"},
    {"wellcome", "welcome"},
    {"wellfare", "welfare"},
    {"wilfull", "willful"},
    {"withold", "withhold"},
    {"writting", "writing"},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const std::u16string word(base::ASCIIToUTF16(kTestCases[i].input));
    EXPECT_FALSE(spellcheck_platform::CheckSpelling(word, 0)) << word;

    // Check if the suggested words occur.
    std::vector<std::u16string> suggestions;
    spellcheck_platform::FillSuggestionList(word, &suggestions);
    bool suggested_word_is_present = false;
    const std::u16string suggested_word(
        base::ASCIIToUTF16(kTestCases[i].suggested_word));
    for (size_t j = 0; j < suggestions.size(); j++) {
      if (suggestions[j].compare(suggested_word) == 0) {
        suggested_word_is_present = true;
        break;
      }
    }
    EXPECT_TRUE(suggested_word_is_present) << suggested_word;
  }
}

// The OSX spellchecker returns non-spellcheck results when invoked on a
// sentence, specifically an NSTextCheckingTypeOrthography result indicating
// the language used in that sentence. Test that it is filtered out from
// RequestTextCheck results.
TEST_F(SpellcheckPlatformMacTest, SpellCheckIgnoresOrthography)  {
  std::u16string test_string(u"Icland is awesome.");
  spellcheck_platform::RequestTextCheck(nullptr, 0, test_string,
                                        std::move(callback_));
  WaitForCallback();
  EXPECT_TRUE(callback_finished_);
  EXPECT_EQ(1U, results_.size());
}

}  // namespace
