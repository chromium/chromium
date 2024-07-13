// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/l10n_util.h"

#include <string>
#include <vector>

#include "chrome/updater/constants.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/installer/exit_code.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {
constexpr int kUpdaterStringIds[] = {
#define HANDLE_STRING(id, ...) id,
    DO_STRING_MAPPING
#undef HANDLE_STRING
};
}  // namespace

TEST(UpdaterL10NUtilTest, GetLocalizedStrings) {
  for (int id : kUpdaterStringIds) {
    ASSERT_FALSE(GetLocalizedString(id).empty());
  }
}

TEST(UpdaterL10NUtilTest, GetLocalizedStringsFormatted) {
  {
    const std::wstring& replacement = L"foobar";
    for (int id : kUpdaterStringIds) {
      std::wstring localized_string_unformatted = GetLocalizedString(id);
      std::wstring localized_string_formatted =
          GetLocalizedStringF(id, replacement);

      ASSERT_FALSE(localized_string_unformatted.empty());
      ASSERT_FALSE(localized_string_formatted.empty());
      if (localized_string_unformatted.find(replacement) != std::string::npos) {
        ASSERT_NE(localized_string_formatted.find(replacement),
                  std::string::npos);
      }
      ASSERT_EQ(localized_string_formatted.find(L"$1"), std::string::npos);
    }
  }

  {
    const std::vector<std::wstring> replacements = {L"foobar", L"replacement",
                                                    L"str"};
    for (int id : kUpdaterStringIds) {
      std::wstring localized_string_unformatted = GetLocalizedString(id);
      std::wstring localized_string_formatted =
          GetLocalizedStringF(id, replacements);
      ASSERT_FALSE(localized_string_unformatted.empty());
      ASSERT_FALSE(localized_string_formatted.empty());

      for (size_t i = 0; i < replacements.size(); ++i) {
        if (localized_string_unformatted.find(replacements[i]) !=
            std::string::npos) {
          ASSERT_NE(localized_string_formatted.find(replacements[i]),
                    std::string::npos);
        }
        std::wstring replacement_str = L"$";
        replacement_str += (L'1' + i);
        ASSERT_EQ(localized_string_formatted.find(replacement_str),
                  std::string::npos);
      }
    }
  }
}

struct UpdaterL10NUtilGetLocalizedStringFCase {
  const std::wstring lang;
  const std::wstring expected_localized_string;
};

class UpdaterL10NUtilGetLocalizedStringF
    : public ::testing::TestWithParam<UpdaterL10NUtilGetLocalizedStringFCase> {
};

INSTANTIATE_TEST_SUITE_P(
    UpdaterL10NUtilGetLocalizedStringFCases,
    UpdaterL10NUtilGetLocalizedStringF,
    ::testing::ValuesIn(std::vector<UpdaterL10NUtilGetLocalizedStringFCase>{
        {L"af", L"Opdateringkontroleringfout: 5555."},
        {L"am", L"የዝማኔ ፍተሻ ስህተት፦ 5555።"},
        {L"ar", L"خطأ في عملية التحقق من التحديث: 5555."},
        {L"as", L"আপডে’ট পৰীক্ষা কৰা সম্পৰ্কীয় আসোঁৱাহ: 5555।"},
        {L"az", L"Yeniləmə yoxlaması xətası: 5555."},
        {L"be", L"Не ўдалося праверыць наяўнасць абнаўленняў: 5555."},
        {L"bg", L"Грешка при проверката за актуализации: 5555."},
        {L"bn", L"আপডেট চেক করার সময় সমস্যা হয়েছে: 5555।"},
        {L"bs", L"Greška provjere ažuriranja: 5555."},
        {L"ca", L"Error de comprovació d'actualització: 5555."},
        {L"cs", L"Chyba kontroly aktualizace: 5555."},
        {L"cy", L"Gwall gwiriad diweddaru: 5555."},
        {L"da", L"Fejl ved søgning efter opdatering: 5555."},
        {L"de", L"Fehler bei der Prüfung auf Updates: 5555."},
        {L"el", L"Σφάλμα ελέγχου για ενημερώσεις: 5555."},
        {L"en-gb", L"Update check error: 5555."},
        {L"en-us", L"Update check error: 5555."},
        {L"es", L"Error de comprobación de actualizaciones: 5555."},
        {L"es-419", L"Error de comprobación de actualizaciones: 5555."},
        {L"et", L"Värskenduse kontrolli viga: 5555."},
        {L"eu", L"Eguneratzearen egiaztapenaren errorea: 5555."},
        {L"fa", L"خطای بررسی به‌روزرسانی: 5555."},
        {L"fi", L"Virhe päivityksiä tarkistettaessa: 5555."},
        {L"fil", L"Error sa pagtingin kung may update: 5555."},
        {L"fr", L"Erreur lors de la recherche de mises à jour : 5555."},
        {L"fr-ca", L"Erreur de vérification de la mise à jour : 5555."},
        {L"gl",
         L"Produciuse un erro durante a comprobación de actualizacións: 5555."},
        {L"gu", L"અપડેટ ચેક કરવામાં ભૂલ: 5555."},
        {L"hi", L"अपडेट जांच में यह गड़बड़ी है: 5555."},
        {L"hr", L"Pogreška provjere ažuriranja: 5555."},
        {L"hu", L"Frissítés-ellenőrzési hiba: 5555."},
        {L"hy", L"Թարմացումների ստուգման սխալ՝ 5555։"},
        {L"id", L"Error pemeriksaan update: 5555."},
        {L"is", L"Villa við uppfærsluleit: 5555."},
        {L"it", L"Errore controllo aggiornamenti: 5555."},
        {L"iw", L"השגיאה בבדיקת העדכון: 5555."},
        {L"ja", L"更新確認エラー: 5555。"},
        {L"ka", L"შეცდომა განახლების შემოწმებისასr: 5555."},
        {L"kk", L"Жаңа нұсқаны тексеру қатесі: 5555."},
        {L"km",
         L"បញ្ហា​ក្នុងការរកមើល​កំណែថ្ម"
         L"ី"
         L"៖"
         L" "
         L"5555។"},
        {L"kn",
         L"ಅಪ್‌ಡೇಟ್‌‌ ಪರಿಶೀಲನೆ ದೋಷ: "
         L"5555."},
        {L"ko", L"업데이트 확인 오류: 5555"},
        {L"ky", L"Жаңыртууну текшерүү катасы: 5555."},
        {L"lo", L"ກວດສອບການອັບເດດຜິດພາດ: 5555."},
        {L"lt", L"Naujinio patikros klaida: 5555."},
        {L"lv", L"Atjauninājumu pārbaudes kļūda: 5555."},
        {L"mk", L"Грешка при проверка за ажурирање: 5555."},
        {L"ml", L"അപ്ഡേറ്റ് പരിശോധനയിലെ പിശക്: 5555."},
        {L"mn", L"Шалгалтын алдааг шинэчлэх: 5555."},
        {L"mr", L"अपडेट तपासणीदरम्यान एरर आली: 5555."},
        {L"ms", L"Ralat semakan kemaskinian: 5555."},
        {L"my", L"အပ်ဒိတ်စစ်ဆေးရန် အမှား- 5555။"},
        {L"ne",
         L"अपडेट उपलब्ध छ कि छैन भन्ने कुरा जाँच्ने क्रममा निम्न त्रुटि "
         L"भयो: 5555।"},
        {L"nl", L"Fout bij updaten: 5555."},
        {L"no", L"Feil ved oppdateringssjekk: 5555."},
        {L"or", L"ଅପଡେଟ ଯାଞ୍ଚରେ ତ୍ରୁଟି: 5555।"},
        {L"pa", L"ਅੱਪਡੇਟ ਦੀ ਜਾਂਚ ਸੰਬੰਧੀ ਗੜਬੜੀ: 5555."},
        {L"pl", L"Błąd sprawdzania aktualizacji: 5555."},
        {L"pt-br", L"Erro na verificação de atualização: 5555."},
        {L"pt-pt", L"Erro na verificação de atualizações: 5555."},
        {L"ro", L"Eroare de verificare a actualizării: 5555."},
        {L"ru", L"Ошибка проверки обновлений: 5555."},
        {L"si", L"යාවත්කාලීන පරීක්ෂාවේ දෝෂය: 5555."},
        {L"sk", L"Chyba kontroly aktualizácií: 5555."},
        {L"sl", L"Napaka pri preverjanju posodobitev: 5555."},
        {L"sq", L"Gabim në kontrollin e përditësimeve: 5555."},
        {L"sr", L"Грешка са провером ажурирања: 5555."},
        {L"sr-latn", L"Greška sa proverom ažuriranja: 5555."},
        {L"sv", L"Fel vid uppdateringskontroll: 5555."},
        {L"sw", L"Hitilafu ya ukaguzi wa sasisho: 5555."},
        {L"ta", L"புதுப்பிப்புச் சரிபார்ப்புப் பிழை: 5555."},
        {L"te",
         L"అప్‌డేట్‌ల కోసం చెక్ "
         L"చేసేటప్పుడు "
         L"ఎర్రర్ "
         L"ఏర్పడింది: "
         L"5555."},
        {L"th", L"ข้อผิดพลาดในการตรวจสอบการอัปเดต: 5555"},
        {L"tr", L"Güncelleme denetimi hatası: 5555."},
        {L"uk", L"Помилка перевірки наявності оновлень: 5555."},
        {L"ur", L"اپ ڈیٹ چیک کرنے میں خرابی: 5555۔"},
        {L"uz", L"Yangilanishni tekshirishda xato: 5555."},
        {L"vi", L"Lỗi khi kiểm tra bản cập nhật: 5555."},
        {L"zh-cn", L"更新检查错误：5555。"},
        {L"zh-hk", L"更新檢查錯誤：5555。"},
        {L"zh-tw", L"更新檢查錯誤：5555。"},
        {L"zu", L"Buyekeza iphutha lokuchekha: 5555."},
    }));

TEST_P(UpdaterL10NUtilGetLocalizedStringF, TestCases) {
  EXPECT_EQ(GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE, L"5555",
                                GetParam().lang),
            GetParam().expected_localized_string);
}

struct GetLocalizedMetainstallerErrorStringTestCase {
  const DWORD exit_code;
  const DWORD windows_error;
  const std::wstring expected_string;
};

class GetLocalizedMetainstallerErrorStringTest
    : public ::testing::TestWithParam<
          GetLocalizedMetainstallerErrorStringTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    GetLocalizedMetainstallerErrorStringTestCases,
    GetLocalizedMetainstallerErrorStringTest,
    ::testing::ValuesIn(std::vector<
                        GetLocalizedMetainstallerErrorStringTestCase>{
        {TEMP_DIR_FAILED, ERROR_ACCESS_DENIED,
         GetLocalizedStringF(IDS_GENERIC_METAINSTALLER_ERROR_BASE,
                             std::vector<std::wstring>{
                                 L"TEMP_DIR_FAILED",
                                 GetTextForSystemError(ERROR_ACCESS_DENIED)})},
        {UNPACKING_FAILED, ERROR_HANDLE_DISK_FULL,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{
                 L"UNPACKING_FAILED",
                 GetTextForSystemError(ERROR_HANDLE_DISK_FULL)})},
        {GENERIC_INITIALIZATION_FAILURE, 0,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{L"GENERIC_INITIALIZATION_FAILURE", {}})},
        {COMMAND_STRING_OVERFLOW, 0,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{L"COMMAND_STRING_OVERFLOW", {}})},
        {WAIT_FOR_PROCESS_FAILED, 0,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{L"WAIT_FOR_PROCESS_FAILED", {}})},
        {PATH_STRING_OVERFLOW, 0,
         GetLocalizedStringF(IDS_GENERIC_METAINSTALLER_ERROR_BASE,
                             std::vector<std::wstring>{L"PATH_STRING_OVERFLOW",
                                                       {}})},
        {UNABLE_TO_GET_WORK_DIRECTORY, 0,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{L"UNABLE_TO_GET_WORK_DIRECTORY", {}})},
        {UNABLE_TO_EXTRACT_ARCHIVE, 0,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{L"UNABLE_TO_EXTRACT_ARCHIVE", {}})},
        {UNEXPECTED_ELEVATION_LOOP, 0,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{L"UNEXPECTED_ELEVATION_LOOP", {}})},
        {UNEXPECTED_DE_ELEVATION_LOOP, 0,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{L"UNEXPECTED_DE_ELEVATION_LOOP", {}})},
        {UNEXPECTED_ELEVATION_LOOP_SILENT, 0,
         GetLocalizedStringF(IDS_GENERIC_METAINSTALLER_ERROR_BASE,
                             std::vector<std::wstring>{
                                 L"UNEXPECTED_ELEVATION_LOOP_SILENT",
                                 {}})},
        {UNABLE_TO_SET_DIRECTORY_ACL, 0,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{L"UNABLE_TO_SET_DIRECTORY_ACL", {}})},
        {INVALID_OPTION, 0,
         GetLocalizedStringF(IDS_GENERIC_METAINSTALLER_ERROR_BASE,
                             std::vector<std::wstring>{L"INVALID_OPTION", {}})},
        {FAILED_TO_DE_ELEVATE_METAINSTALLER, 0,
         GetLocalizedStringF(IDS_GENERIC_METAINSTALLER_ERROR_BASE,
                             std::vector<std::wstring>{
                                 L"FAILED_TO_DE_ELEVATE_METAINSTALLER",
                                 {}})},
        {RUN_SETUP_FAILED_FILE_NOT_FOUND, 0,
         GetLocalizedStringF(IDS_GENERIC_METAINSTALLER_ERROR_BASE,
                             std::vector<std::wstring>{
                                 L"RUN_SETUP_FAILED_FILE_NOT_FOUND",
                                 {}})},
        {RUN_SETUP_FAILED_PATH_NOT_FOUND, 0,
         GetLocalizedStringF(IDS_GENERIC_METAINSTALLER_ERROR_BASE,
                             std::vector<std::wstring>{
                                 L"RUN_SETUP_FAILED_PATH_NOT_FOUND",
                                 {}})},
        {RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS, 0,
         GetLocalizedStringF(IDS_GENERIC_METAINSTALLER_ERROR_BASE,
                             std::vector<std::wstring>{
                                 L"RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS",
                                 {}})},
        {UNABLE_TO_GET_EXE_PATH, 0,
         GetLocalizedStringF(
             IDS_GENERIC_METAINSTALLER_ERROR_BASE,
             std::vector<std::wstring>{L"UNABLE_TO_GET_EXE_PATH", {}})},
        {UNSUPPORTED_WINDOWS_VERSION, 0,
         GetLocalizedString(IDS_UPDATER_OS_NOT_SUPPORTED_BASE)},
        {FAILED_TO_ELEVATE_METAINSTALLER, ERROR_CANCELLED,
         GetLocalizedStringF(IDS_FAILED_TO_ELEVATE_METAINSTALLER_BASE,
                             GetTextForSystemError(ERROR_CANCELLED))},
    }));

TEST_P(GetLocalizedMetainstallerErrorStringTest, TestCases) {
  ASSERT_EQ(GetLocalizedMetainstallerErrorString(GetParam().exit_code,
                                                 GetParam().windows_error),
            GetParam().expected_string);
}

}  // namespace updater
