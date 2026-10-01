# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .allow_deleting_browser_history.allow_deleting_browser_history import *  # noqa: F403
from .apps_shortcut.apps_shortcut import *  # noqa: F403
from .bookmarkbar_enabled.bookmarkbar_enabled import *  # noqa: F403
from .chrome_data_region_setting.chrome_data_region_setting import *  # noqa: F403
from .cloud_management_enrollment_token.cloud_management_enrollment_token import *  # noqa: F403
from .cloud_reporting_enabled.cloud_reporting_enabled import *  # noqa: F403
from .default_search_provider.default_search_provider import *  # noqa: F403
from .extension_blocklist.extension_blocklist import *  # noqa: F403
from .extension_forcelist.extension_forcelist import *  # noqa: F403
from .extension_allowlist.extension_allowlist import *  # noqa: F403
from .force_google_safe_search.force_google_safe_search import *  # noqa: F403
from .gemini_settings.gemini_settings import *  # noqa: F403

# Disable fullscreenallowed test due to pywinauto infra issue http://b/259118140
# from .fullscreen_allowed.fullscreen_allowed import *
from .homepage.homepage import *  # noqa: F403
from .mergelist.mergelist import *  # noqa: F403
from .password_manager_enabled.password_manager_enabled import *  # noqa: F403
from .popups_allowed.popups_allowed import *  # noqa: F403
from .precedence.precedence import *  # noqa: F403
from .encrypted_reporting.report_cbcm_events import *  # noqa: F403
from .restore_on_startup.restore_on_startup import *  # noqa: F403

# Disable safe_browsing test due to chrome://downloads shadow DOM issue http://b/298889715
# from .safe_browsing.safe_browsing import *
from .translate_enabled.translate_enabled import *  # noqa: F403
from .url_blocklist.url_blocklist import *  # noqa: F403
from .url_allowlist.url_allowlist import *  # noqa: F403
from .user_data_dir.user_data_dir import *  # noqa: F403
from .webprotect_file_download.webprotect_file_download import *  # noqa: F403
from .youtube_restrict.youtube_restrict import *  # noqa: F403
