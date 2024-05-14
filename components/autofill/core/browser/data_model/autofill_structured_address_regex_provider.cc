// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"

#include "base/notreached.h"

namespace autofill {

namespace {

// Best practices for writing regular expression snippets:
// By wrapping snippets in non-capture groups, i.e. (?: ... ), we ensure that a
// pending "?" is interpreted as "optional" instead of a modifier of a previous
// operator. E.g. `StrCat({"(?:a+)", "?"})` means an optional sequence of "a"
// characters. But `StrCat({"a+", "?"})` means lazily match one or more "a"
// characters. Prefer [^\s,] ('not a whitespace or a comma') over \w ('a word
// character') in names, when you have concerns about hyphens (e.g. the German
// name "Hans-Joachim") because '-' is not matched by \w.

// Regular expressions pattern of common two-character CJK last names.
// Korean names are written in Hangul.
// Chinese names are written in their traditional and simplified version.
// Source:
// https://en.wikipedia.org/wiki/List_of_Korean_surnames
// https://zh.wikipedia.org/wiki/%E8%A4%87%E5%A7%93#.E5.B8.B8.E8.A6.8B.E7.9A.84.E8.A4.87.E5.A7.93
const char kTwoCharacterCjkLastNamesRe[] =
    "(?:남궁|사공|서문|선우|제갈|황보|독고|망절"
    "|欧阳|令狐|皇甫|上官|司徒|诸葛|司马|宇文|呼延|端木"
    "|張簡|歐陽|諸葛|申屠|尉遲|司馬|軒轅|夏侯)";

// Regular expression pattern for a Hangul (Korean) character.
const char kHangulCharacterRe[] = "(?:\\p{Hangul})";

// Regular expression pattern for a sequence of Hangul (Korean) character.
const char kHangulCharactersRe[] = "(?:\\p{Hangul}+)";

// Regular expression pattern to match separators as used in CJK names:
// Included separators: \u30FB, \u00B7, \u3000 or a simple space.
const char kCjkNameSeperatorsRe[] = "(?:・|·|　|\\s+)";

// Regular expression pattern for common honorific name prefixes.
// The list is incomplete and focused on the English and German language.
// Sources:
// * https://en.wikipedia.org/wiki/English_honorifics
// * https://en.wikipedia.org/wiki/German_honorifics
// TODO(crbug.com/1107770): Include more languages and categories.
const char kHonorificPrefixRe[] =
    "(?:"
    "Master|Mr\\.?|Miss\\.?|Mrs\\.?|Missus|Ms\\.?|Mx\\.?|M\\.?|Ma'am|Sir|"
    "Gentleman|Sire|Mistress|Madam|Ma'am|Dame|Lord|Lady|Esq|Excellency|"
    "Excellence|Her Honour|His Honour|Hon\\.?|The Right Honourable|The Most "
    "Honourable|Dr\\.?|PhD|DPhil|MD|DO|Prof\\.|Professor|QC|CL|Chancellor|Vice-"
    "Chancellor|Principle|Principal|President|Master|Warden|Dean|Regent|Rector|"
    "Provost|Director|Chief Executive|Imām|Shaykh|Muftī|Hāfiz|Hāfizah|Qārī"
    "|Mawlānā|Hājī|Sayyid|Sayyidah|Sharif|Eminent|Venerable|His Holiness"
    "|His Holiness|His All Holiness|His Beatitude|The Most Blessed"
    "|His Excellency|His Most Eminent Highness|His Eminence"
    "|Most Reverend Eminence|The Most Reverend|His Grace|His Lordship"
    "|The Reverend|Fr|Pr|Br|Sr|Elder|Rabbi|The Reverend|Cantor|Chief Rabbi"
    "|Grand "
    "Rabbi|Rebbetzin|Herr|Frau|Fräulein|Dame|PD|Doktor|Magister|Ingenieur"
    "|1lt|1st|2lt|2nd|3rd|admiral|capt|captain|col|cpt|dr|gen|general|lcdr"
    "|lt|ltc|ltg|ltjg|maj|major|mg|pastor|prof|rep|reverend"
    "|rev|sen|st)";

// Regular expression pattern for an optional last name suffix.
const char kOptionalLastNameSuffixRe[] =
    "(?:(?:(?:b\\.a|ba|d\\.d\\.s|dds|ii|iii|iv|ix|jr|m\\.a|m\\.d|md|ms|"
    "ph\\.?d|sr|v|vi|vii|viii|x)\\.?)?)";

// Regular expression pattern for a CJK character.
const char kCjkCharacterRe[] =
    "(?:"
    "\\p{Han}|"
    "\\p{Hangul}|"
    "\\p{Katakana}|"
    "\\p{Hiragana}|"
    "\\p{Bopomofo})";

// Regular expression pattern for a sequence of CJK character.
const char kCjkCharactersRe[] =
    "(?:(?:"
    "\\p{Han}|"
    "\\p{Hangul}|"
    "\\p{Katakana}|"
    "\\p{Hiragana}|"
    "\\p{Bopomofo})+)";

// Regular expression pattern of common two-character Korean names.
// Korean last names are written in Hangul. Note, some last names are ambiguous
// in the sense that they share a common prefix with a single-character last
// name. Source: https://en.wikipedia.org/wiki/List_of_Korean_surnames
const char kTwoCharacterKoreanNamesRe[] =
    "(?:강전|남궁|독고|동방|망절|사공|서문|선우"
    "|소봉|어금|장곡|제갈|황목|황보)";

// Regular expression pattern to match if a string contains a common
// Hispanic/Latinx last name.
// It contains the most common names in Spain, Mexico, Cuba, Dominican Republic,
// Puerto Rico and Guatemala.
// Source: https://en.wikipedia.org/wiki/List_of_common_Spanish_surnames
const char kHispanicCommonLastNameCharacteristicsRe[] =
    "(?:Aguilar|Alonso|Álvarez|Amador|Betancourt|Blanco|Burgos|Castillo|Castro|"
    "Chávez|Colón|Contreras|Cortez|Cruz|Delgado|Diaz|Díaz|Domínguez|Estrada|"
    "Fernandez|Fernández|Flores|Fuentes|Garcia|García|Garza|Gil|Gómez|González|"
    "Guerrero|Gutiérrez|Guzmán|Hernández|Herrera|Iglesias|Jiménez|Juárez|Lopez|"
    "López|Luna|Marín|Marroquín|Martín|Martinez|Martínez|Medina|Méndez|Mendoza|"
    "Molina|Morales|Moreno|Muñoz|Narvaez|Navarro|Núñez|Ortega|Ortiz|Ortíz|Peña|"
    "Perez|Pérez|Ramírez|Ramos|Reyes|Rivera|Rodriguez|Rodríguez|Rojas|Romero|"
    "Rosario|Rubio|Ruiz|Ruíz|Salazar|Sanchez|Sánchez|Santana|Santiago|Santos|"
    "Sanz|Serrano|Soto|Suárez|Toro|Torres|Vargas|Vasquez|Vásquez|Vázquez|"
    "Velásquez)";

// Regular expression pattern to match a single word.
const char kSingleWordRe[] = "(?:[^\\s,]+)";

// Regular expression pattern for multiple lazy words meaning that the
// expression avoids to match more than one word if possible.
// Words are separated by white spaces but not by newlines or carriage returns.
const char kMultipleLazyWordsRe[] = "(?:[^\\s,]+(?:[^\\S\\r\\n]+[^\\s,]+)*?)";

// Regular expression pattern to check if a name contains a Hispanic/Latinx
// last name conjunction.
const char kHispanicLastNameConjunctionCharacteristicsRe[] = "\\s(y|e|i)\\s";

// Regular expression pattern to match the conjunction used between
// Hispanic/Latinx last names.
const char kHispanicLastNameConjunctionsRe[] = "(?:y|e|i)";

// Regular expression pattern to match common prefixes belonging to a (single)
// last name.
// Source: https://en.wikipedia.org/wiki/List_of_family_name_affixes
// According to the source, the list is partial. Changes to the list:
// * "De la" and "De le" is added to support the combination of "de" and
// "le"/"la" as used in Hispanic/Latinx names.
// * The matching of "i" is made lazy to give the last name conjunction
// precedence.
const char kOptionalLastNamePrefixRe[] =
    "(?:(?:"
    "a|ab|af|av|ap|abu|aït|al|ālam|aust|austre|bar|bath|bat|ben|bin|ibn|bet|"
    "bint|binti|binte|da|das|de|degli|dele|del|du|della|der|di|dos|du|e|el|"
    "fetch|vetch|fitz|i??|kil|gil|de le|de "
    "la|la|le|lille|lu|m|mac|mc|mck|mhic|mic|mala|"
    "mellom|myljom|na|ned|nedre|neder|nic|ni|nin|nord|norr|ny|o|ua|"
    "ui|opp|upp|öfver|ost|öst|öster|øst|øst|østre|över|øvste|øvre|øver|öz|pour|"
    "putra|putri|setia|tor|söder|sør|sønder|sør|syd|søndre|syndre|søre|ter|ter|"
    "tre|van|van der|väst|väster|verch|erch|vest|vestre|vesle|vetle|von|zu|von "
    "und zu)\\s)?";

// Regular expression to match the affixes that indicate the floor an
// apartment is located in.
const char kFloorAffixRe[] =
    "((°|º|\\.|\\s|-)*"
    "(floor|flur|fl|og|obergeschoss|ug|untergeschoss|geschoss|andar|piso|º)"
    "(\\.|\\s|-)*)";

// Prefix that indicates an apartment number.
const char kApartmentNumberPrefix[] =
    "((apt|apartment|wohnung|apto|-)(\\.|\\s|-)*)";

// Suffix that inficates an apartment number.
const char kApartmentNumberSuffix[] = "(\\.|\\s|-)*(ª)";

// Regular expression to match the prefixes that indicate a house number.
const char kHouseNumberOptionalPrefixRe[] = "(((no|°|º|number)(\\.|-|\\s)*)?)";

// Regular expressions to characterize if a string contains initials by
// checking that:
// * The string contains only upper case letters that may be preceded by a
// point.
// * Between each letter, there can be a space or a hyphen.
const char kMiddleNameInitialsCharacteristicsRe[] =
    "^(?:[A-Z]\\.?(?:(?:\\s|-)?[A-Z]\\.?)*)$";

// Returns an expression to parse a CJK name that includes one separator.
// The full name is parsed into |NAME_FULL|, the part of the name before the
// separator is parsed into |NAME_LAST| and the part after the separator is
// parsed into |NAME_FIRST|.
std::string ParseSeparatedCJkNameExpression() {
  return CaptureTypeWithPattern(
      NAME_FULL,
      {// Parse one or more CJK characters into the last name.
       CaptureTypeWithPattern(NAME_LAST, kCjkCharactersRe,
                              {.separator = kCjkNameSeperatorsRe}),
       // Parse the remaining CJK characters into the first name.
       CaptureTypeWithPattern(NAME_FIRST, kCjkCharactersRe)});
}

// Returns an expression to parse a CJK name that starts with a known
// two-character last name.
std::string ParseCommonCjkTwoCharacterLastNameExpression() {
  return CaptureTypeWithPattern(
      NAME_FULL,
      {// Parse known two-character CJK last name into |NAME_LAST|.
       CaptureTypeWithPattern(NAME_LAST, kTwoCharacterCjkLastNamesRe,
                              {.separator = std::string()}),
       // Parse the remaining CJK characters into |NAME_FIRST|.
       CaptureTypeWithPattern(
           NAME_FIRST, kCjkCharactersRe,
           {.separator = "", .quantifier = MatchQuantifier::kOptional})});
}

// Returns an expression to parse a CJK name without a separator.
// The full name is parsed into |NAME_FULL|, the first character is parsed
// into |NAME_LAST| and the rest into |NAME_FIRST|.
std::string ParseCjkSingleCharacterLastNameExpression() {
  return CaptureTypeWithPattern(
      NAME_FULL,
      {// Parse the first CJK character into |NAME_LAST|.
       CaptureTypeWithPattern(NAME_LAST, kCjkCharacterRe,
                              {.separator = std::string()}),
       // Parse the remaining CJK characters into |NAME_FIRST|.
       CaptureTypeWithPattern(
           NAME_FIRST, kCjkCharactersRe,
           {.separator = "", .quantifier = MatchQuantifier::kOptional})});
}

// Returns an expression to parse a Korean name that contains at least 4
// characters with a common Korean two-character last name. The full name is
// parsed into |NAME_FULL|, the first two characters into |NAME_LAST| and the
// rest into |NAME_FIRST|.
std::string ParseKoreanTwoCharacterLastNameExpression() {
  return CaptureTypeWithPattern(
      NAME_FULL,
      {// Parse known Korean two-character last names into |NAME_LAST|.
       CaptureTypeWithPattern(NAME_LAST, kTwoCharacterKoreanNamesRe,
                              {.separator = std::string()}),
       // Parse at least two remaining Hangul characters into
       // |NAME_FIRST|.
       CaptureTypeWithPattern(NAME_FIRST,
                              {kHangulCharacterRe, kHangulCharactersRe})});
}

// Returns an expression to determine if a name has the characteristics of a
// CJK name.
std::string MatchCjkNameExpression() {
  return base::StrCat({// Must contain one or more CJK characters
                       "^", kCjkCharactersRe,
                       // Followed by an optional separator with one
                       // or more additional CJK characters.
                       "(", kCjkNameSeperatorsRe, kCjkCharactersRe, ")?$"});
}

// Returns an expression to parse a full name that contains only a last name.
std::string ParseOnlyLastNameExpression() {
  return CaptureTypeWithPattern(
      NAME_FULL, {CaptureTypeWithPattern(
                      NAME_LAST, {kOptionalLastNamePrefixRe, kSingleWordRe}),
                  kOptionalLastNameSuffixRe});
}

// Returns an expression to parse a name that consists of a first, middle and
// last name with an optional honorific prefix. The full name is parsed into
// |NAME_FULL|. The name can start with an honorific prefix that is ignored.
// The last token is parsed into |NAME_LAST|.
// This token may be preceded by a last name prefix like "Mac" or
// "von" that is included in |NAME_LAST|. If the strings contains any
// remaining tokens, the first token is parsed into
// |NAME_FIRST| and all remaining tokens into |NAME_MIDDLE|.
std::string ParseFirstMiddleLastNameExpression() {
  return CaptureTypeWithPattern(
      NAME_FULL,
      {NoCapturePattern(
           kHonorificPrefixRe,
           CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
       CaptureTypeWithPattern(
           NAME_FIRST, kSingleWordRe,
           CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
       CaptureTypeWithPattern(
           NAME_MIDDLE, kMultipleLazyWordsRe,
           CaptureOptions{.quantifier = MatchQuantifier::kLazyOptional}),
       CaptureTypeWithPattern(NAME_LAST,
                              {kOptionalLastNamePrefixRe, kSingleWordRe}),
       kOptionalLastNameSuffixRe});
}

// Returns an expression to parse a name that starts with the last name,
// followed by a comma, and than the first and middle names.
// The full name is parsed into |NAME_FULL|. The name can start with an optional
// honorific prefix that is ignored, followed by a single
// token that is parsed into |LAST_NAME|. The |LAST_NAME| must be preceded by a
// comma with optional spaces. The next token is parsed into |NAME_FIRST| and
// all remaining tokens are parsed into |NAME_MIDDLE|.
std::string ParseLastCommaFirstMiddleExpression() {
  return CaptureTypeWithPattern(
      NAME_FULL,
      {NoCapturePattern(
           kHonorificPrefixRe,
           CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
       CaptureTypeWithPattern(NAME_LAST,
                              {kOptionalLastNamePrefixRe, kSingleWordRe},
                              {.separator = "\\s*,\\s*"}),
       CaptureTypeWithPattern(
           NAME_FIRST, kSingleWordRe,
           CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
       CaptureTypeWithPattern(
           NAME_MIDDLE, kMultipleLazyWordsRe,
           CaptureOptions{.quantifier = MatchQuantifier::kLazyOptional})});
}

// Returns an expression to parse an Hispanic/Latinx last name.
// The last name can consist of two parts with an optional conjunction.
// The full last name is parsed into |NAME_LAST|, the first part into
// |NAME_LAST_FIRST|, the conjunction into |NAME_LAST_CONJUNCTION|, and the
// second part into |NAME_LAST_SECOND|.
// Each last name part consists of a space-separated toke with an optional
// prefix like "de le". If only one last name part is found, it is parsed into
// |NAME_LAST_SECOND|.
std::string ParseHispanicLastNameExpression() {
  return CaptureTypeWithPattern(
      NAME_LAST,
      {CaptureTypeWithPattern(NAME_LAST_FIRST,
                              {kOptionalLastNamePrefixRe, kSingleWordRe}),
       CaptureTypeWithPattern(
           NAME_LAST_CONJUNCTION, kHispanicLastNameConjunctionsRe,
           CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
       CaptureTypeWithPattern(NAME_LAST_SECOND,
                              {kOptionalLastNamePrefixRe, kSingleWordRe})});
}

// Returns an expression to parse a full Hispanic/Latinx name that
// contains an optional honorific prefix which is ignored, a first name, and a
// last name as specified by |ParseHispanicLastNameExpression()|.
std::string ParseHispanicFullNameExpression() {
  return CaptureTypeWithPattern(
      NAME_FULL,
      {NoCapturePattern(
           kHonorificPrefixRe,
           CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
       CaptureTypeWithPattern(
           NAME_FIRST, kMultipleLazyWordsRe,
           CaptureOptions{.quantifier = MatchQuantifier::kLazyOptional}),
       ParseHispanicLastNameExpression()});
}

// Returns an expression that parses the whole |LAST_NAME| into
// |LAST_NAME_SECOND|.
std::string ParseLastNameIntoSecondLastNameExpression() {
  return CaptureTypeWithPattern(
      NAME_LAST,
      {CaptureTypeWithPattern(NAME_LAST_SECOND, kMultipleLazyWordsRe)});
}

// Returns an expression to parse a street address into the street name, the
// house number and the subpremise. The latter is parsed into the floor and
// apartment number. The expression is applicable, if the street name comes
// before the house number, followed by the floor and the apartment.
// Both the floor and the apartment must be indicated by a prefix.
// Example: Erika-Mann-Str. 44, Floor 2, Apartment 12
std::string ParseStreetNameHouseNumberExpression() {
  return CaptureTypeWithPattern(
      ADDRESS_HOME_STREET_ADDRESS,
      {CaptureTypeWithPattern(
           ADDRESS_HOME_STREET_LOCATION,
           {CaptureTypeWithPattern(ADDRESS_HOME_STREET_NAME,
                                   kMultipleLazyWordsRe),
            CaptureTypeWithAffixedPattern(ADDRESS_HOME_HOUSE_NUMBER,
                                          kHouseNumberOptionalPrefixRe,
                                          "(?:\\d+\\w?)", "(th\\.|\\.)?")},
           CaptureOptions{.separator = ""}),
       CaptureTypeWithPattern(
           ADDRESS_HOME_SUBPREMISE,
           {
               CaptureTypeWithPrefixedPattern(
                   ADDRESS_HOME_FLOOR, kFloorAffixRe, "(?:(\\d{1,3}\\w?|\\w))",
                   CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
               CaptureTypeWithPrefixedPattern(
                   ADDRESS_HOME_APT_NUM, kApartmentNumberPrefix,
                   "(?:(\\d{1,3}\\w?|\\w))",
                   CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
           },
           CaptureOptions{.quantifier = MatchQuantifier::kOptional})});
}

// Returns an expression to parse a street address into the street name, the
// house number and the subpremise. The latter is parsed into the floor and
// apartment number. The expression is applicable, if the street name comes
// before the house number, followed by the floor and the apartment.
// Both the floor and the apartment must be indicated by a suffix.
// Example: Calla 1, 2º, 3ª
// Where 2 is the floor and 3 the apartment number.
std::string ParseStreetNameHouseNumberSuffixedFloorAndAppartmentExpression() {
  return CaptureTypeWithPattern(
      ADDRESS_HOME_STREET_ADDRESS,
      {CaptureTypeWithPattern(
           ADDRESS_HOME_STREET_LOCATION,
           {CaptureTypeWithPattern(ADDRESS_HOME_STREET_NAME,
                                   kMultipleLazyWordsRe),
            CaptureTypeWithAffixedPattern(ADDRESS_HOME_HOUSE_NUMBER,
                                          kHouseNumberOptionalPrefixRe,
                                          "(?:\\d+\\w?)", "(th\\.|\\.)?")},
           CaptureOptions{.separator = ""}),

       CaptureTypeWithPattern(
           ADDRESS_HOME_SUBPREMISE,
           {
               CaptureTypeWithSuffixedPattern(
                   ADDRESS_HOME_FLOOR, "(?:(\\d{1,3}\\w?|\\w))", kFloorAffixRe,
                   CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
               CaptureTypeWithAffixedPattern(
                   ADDRESS_HOME_APT_NUM, "(-\\s*)?", "(?:(\\d{1,3}\\w?|\\w))",
                   kApartmentNumberSuffix,
                   CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
           },
           CaptureOptions{.quantifier = MatchQuantifier::kOptional})});
}

// Returns an expression to parse a street address into the street name, the
// house number and the subpremise. The latter is parsed into the floor and
// apartment number. The expression is applicable, if the house number comes
// before the street name, followed by the floor which is indicated by a suffix
// and the apartment.
// Example Av. Paulista, 1098, 1º andar, apto. 101
std::string ParseStreetNameHouseNumberExpressionSuffixedFloor() {
  return CaptureTypeWithPattern(
      ADDRESS_HOME_STREET_ADDRESS,
      {

          CaptureTypeWithPattern(
              ADDRESS_HOME_STREET_LOCATION,
              {CaptureTypeWithPattern(ADDRESS_HOME_STREET_NAME,
                                      kMultipleLazyWordsRe),
               CaptureTypeWithAffixedPattern(ADDRESS_HOME_HOUSE_NUMBER,
                                             kHouseNumberOptionalPrefixRe,
                                             "(?:\\d+\\w?)", "(th\\.|\\.)?")},
              {.separator = ""}),

          CaptureTypeWithPattern(
              ADDRESS_HOME_SUBPREMISE,
              {
                  CaptureTypeWithSuffixedPattern(
                      ADDRESS_HOME_FLOOR, "(?:(\\d{0,3}\\w?))", kFloorAffixRe,
                      CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
                  CaptureTypeWithPrefixedPattern(
                      ADDRESS_HOME_APT_NUM, kApartmentNumberPrefix,
                      "(?:(\\d{0,3}\\w?))",
                      CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
              },
              CaptureOptions{.quantifier = MatchQuantifier::kOptional})});
}

// Returns an expression to parse a street address into the street name, the
// house number and the subpremise. The latter is parsed into the floor and
// apartment number. The expression is applicable, if the house number comes
// before the street name, followed by the floor and the apartment.
// Both the floor and the apartment must be indicated by a prefix.
// Example: 1600 Main Avenue, Floor 2, Apartment 12
std::string ParseHouseNumberStreetNameExpression() {
  return CaptureTypeWithPattern(
      ADDRESS_HOME_STREET_ADDRESS,
      {CaptureTypeWithPattern(
           ADDRESS_HOME_STREET_LOCATION,
           {CaptureTypeWithAffixedPattern(ADDRESS_HOME_HOUSE_NUMBER,
                                          kHouseNumberOptionalPrefixRe,
                                          "(?:\\d+\\w?)", "(th\\.|\\.)?"),
            CaptureTypeWithPattern(ADDRESS_HOME_STREET_NAME,
                                   kMultipleLazyWordsRe)},
           {.separator = ""}),
       CaptureTypeWithPattern(
           ADDRESS_HOME_SUBPREMISE,
           {
               CaptureTypeWithPrefixedPattern(
                   ADDRESS_HOME_FLOOR, kFloorAffixRe, "(?:(\\d{0,3}\\w?))",
                   CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
               CaptureTypeWithPrefixedPattern(
                   ADDRESS_HOME_APT_NUM, kApartmentNumberPrefix,
                   "(?:(\\d{0,3}\\w?))",
                   CaptureOptions{.quantifier = MatchQuantifier::kOptional}),
           },
           CaptureOptions{.quantifier = MatchQuantifier::kOptional})});
}
}  // namespace

StructuredAddressesRegExProvider::StructuredAddressesRegExProvider() = default;

// static
StructuredAddressesRegExProvider* StructuredAddressesRegExProvider::Instance() {
  static base::NoDestructor<StructuredAddressesRegExProvider>
      g_expression_provider;
  return g_expression_provider.get();
}

std::string StructuredAddressesRegExProvider::GetPattern(
    RegEx expression_identifier,
    const std::string& country_code) {
  switch (expression_identifier) {
    case RegEx::kSingleWord:
      return kSingleWordRe;
    case RegEx::kParseSeparatedCjkName:
      return ParseSeparatedCJkNameExpression();
    case RegEx::kParseCommonCjkTwoCharacterLastName:
      return ParseCommonCjkTwoCharacterLastNameExpression();
    case RegEx::kParseKoreanTwoCharacterLastName:
      return ParseKoreanTwoCharacterLastNameExpression();
    case RegEx::kParseCjkSingleCharacterLastName:
      return ParseCjkSingleCharacterLastNameExpression();
    case RegEx::kMatchHispanicCommonNameCharacteristics:
      return kHispanicCommonLastNameCharacteristicsRe;
    case RegEx::kMatchHispanicLastNameConjuctionCharacteristics:
      return kHispanicLastNameConjunctionCharacteristicsRe;
    case RegEx::kMatchCjkNameCharacteristics:
      return MatchCjkNameExpression();
    case RegEx::kParseOnlyLastName:
      return ParseOnlyLastNameExpression();
    case RegEx::kParseLastCommaFirstMiddleName:
      return ParseLastCommaFirstMiddleExpression();
    case RegEx::kParseFirstMiddleLastName:
      return ParseFirstMiddleLastNameExpression();
    case RegEx::kParseHispanicLastName:
      return ParseHispanicLastNameExpression();
    case RegEx::kParseHispanicFullName:
      return ParseHispanicFullNameExpression();
    case RegEx::kMatchMiddleNameInitialsCharacteristics:
      return kMiddleNameInitialsCharacteristicsRe;
    case RegEx::kParseLastNameIntoSecondLastName:
      return ParseLastNameIntoSecondLastNameExpression();
    case RegEx::kParseHouseNumberStreetName:
      return ParseHouseNumberStreetNameExpression();
    case RegEx::kParseStreetNameHouseNumberSuffixedFloor:
      return ParseStreetNameHouseNumberExpressionSuffixedFloor();
    case RegEx::kParseStreetNameHouseNumberSuffixedFloorAndApartmentRe:
      return ParseStreetNameHouseNumberSuffixedFloorAndAppartmentExpression();
    case RegEx::kParseStreetNameHouseNumber:
      return ParseStreetNameHouseNumberExpression();
  }
  NOTREACHED_IN_MIGRATION();
}

const RE2* StructuredAddressesRegExProvider::GetRegEx(
    RegEx expression_identifier,
    const std::string& country_code) {
  base::AutoLock lock(lock_);
  auto it = cached_expressions_.find(expression_identifier);
  if (it == cached_expressions_.end()) {
    std::unique_ptr<const RE2> expression =
        BuildRegExFromPattern(GetPattern(expression_identifier, country_code));
    const RE2* expresstion_ptr = expression.get();
    cached_expressions_.emplace(expression_identifier, std::move(expression));
    return expresstion_ptr;
  }
  return it->second.get();
}

}  // namespace autofill
