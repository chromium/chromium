# Solutions for Chrome Page Controller Code Lab

## These are sample solutions for the Code Lab, but not the only way to do it.

***Paths in steps 1-4 are relative to src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/,
   steps 5 is relative the chromium/src.***

1) tests/codelab/SettingsForCodelabTest.java:
``` java
package org.chromium.chrome.test.pagecontroller.tests.codelab;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.pagecontroller.controllers.codelab.SearchEngineSelectionControllerForCodelab;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.ChromeMenu;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;

/**
 * Test for Page Controllers created in the code lab.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class SettingsForCodelabTest {
    // ChromeUiAutomatorTestRule will capture a screen shot and UI Hierarchy info in the event
    // of a test failure to aid test debugging.
    public ChromeUiAutomatorTestRule mUiAutomatorRule = new ChromeUiAutomatorTestRule();

    // ChromeUiApplicationTestRule provides a way to launch the Chrome Application under test
    // and access to the Page Controllers.
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();

    // The rule chain allows deterministic ordering of test rules so that the
    // UiAutomatorRule will print debugging information in case of errors
    // before the application is shut-down.
    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mUiAutomatorRule);

    private ChromeMenu mChromeMenu;

    @Before
    public void setUp() {
        // TODO: Obtain a ChromeMenu instance.  Hint, start with
        //       mChromeUiRule.launchIntoNewTabPageOnFirstRun().
        mChromeMenu = mChromeUiRule.launchIntoNewTabPageOnFirstRun().openChromeMenu();
    }

    @Test
    public void testOpenSettingsControllerForCodelab() {
        // TODO: Modify ChromeMenu.java to add a method that returns an
        //       instance of CodeLabSettings Page Controller.
        mChromeMenu.openSettingsForCodelab();
    }

    @Test
    public void testSwitchSearchEngine() {
        // TODO: Write a test to verify ability to change the default search
        //       engine.
        SearchEngineSelectionControllerForCodelab engineSelection =
                mChromeMenu.openSettingsForCodelab().clickSearchEngine();
        Assert.assertEquals(engineSelection.getEngineChoice(), "Google");
        engineSelection.chooseSearchEngine("Bing");
        Assert.assertEquals(engineSelection.getEngineChoice(), "Bing");
    }
}
```

2) controllers/codelab/SettingsControllerForCodelab.java:
``` java
package org.chromium.chrome.test.pagecontroller.controllers.codelab;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

// TODO: Read documentation in the PageController class.  Refer to implemented
// Page Controllers in the pagecontroller directory for examples.
/**
 * Settings Menu Page Controller for the Code Lab.
 */
public class SettingsControllerForCodelab extends PageController {
    // TODO: Replace null with a an actual locator.  (Hint, see Ui2Locators.with*.)
    private static final IUi2Locator LOCATOR_SETTINGS =
            Ui2Locators.withTextString(R.string.settings);

    // TODO: (Hint, you may need more IUi2Locators than just LOCATOR_SETTINGS,
    // add them here.
    private static final IUi2Locator LOCATOR_SEARCH_ENGINE =
            Ui2Locators.withTextString(R.string.search_engine_settings);

    // The next 5 lines are boilerplate, no need to modify.
    private static final SettingsControllerForCodelab sInstance =
            new SettingsControllerForCodelab();
    private SettingsControllerForCodelab() {}
    public static SettingsControllerForCodelab getInstance() {
        return sInstance;
    }

    @Override
    public SettingsControllerForCodelab verifyActive() {
        // TODO: See PageController.verifyActive documentation on what this
        // method should do.  (Hint, PageController has a mLocatorHelper field.)
        mLocatorHelper.verifyOnScreen(LOCATOR_SETTINGS);

        return this;
    }

    /**
     * Click on the Search Engine option.
     * @returns SearchEngineSelectionControllerForCodelab PageController.
     */
    public SearchEngineSelectionControllerForCodelab clickSearchEngine() {
        // TODO: Perform a click on the Search engine option
        // in the Settings menu.  (Hint, PageController has a mUtils field.)
        mUtils.click(LOCATOR_SEARCH_ENGINE);

        // TODO: Replace null with an instance of
        // SearchEngineSelectionControllerForCodelab.  (Hint, all PageController
        // subclasses have a verifyActive method.)
        return SearchEngineSelectionControllerForCodelab.getInstance().verifyActive();
    }
}
```

3) controllers/codelab/SearchEngineSelectionControllerForCodelab.java:
``` java
package org.chromium.chrome.test.pagecontroller.controllers.codelab;

import android.support.test.uiautomator.UiObject2;
import android.util.Pair;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiLocationException;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

import java.util.List;

/**
 * Search Engine Selection Page Controller for the Code Lab, corresponds to
 * {@link org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings}.
 */
public class SearchEngineSelectionControllerForCodelab extends PageController {
    // TODO: Put locators here.
    private static final IUi2Locator LOCATOR_SEARCH_ENGINE =
            Ui2Locators.withPath(Ui2Locators.withAnyResEntry(R.id.action_bar),
                    Ui2Locators.withTextString(R.string.search_engine_settings));
    private static final IUi2Locator LOCATOR_ALL_ENGINES = Ui2Locators.withPath(
            Ui2Locators.withAnyResEntry(R.id.name), Ui2Locators.withTextRegex("^(.+)$"));
    private class EngineSelectionIndicator
            implements UiLocatorHelper.CustomElementMaker<Pair<String, Boolean>> {
        @Override
        public Pair<String, Boolean> makeElement(UiObject2 root, boolean isLastAttempt) {
            String engine = mLocatorHelper.getOneTextImmediate(LOCATOR_ALL_ENGINES, root);
            Boolean isChecked = mLocatorHelper.getOneCheckedImmediate(LOCATOR_ALL_ENGINES, root);
            if (engine == null) {
                throw new UiLocationException("Engine not found.", LOCATOR_ALL_ENGINES, root);
            } else if (isChecked == null) {
                throw new UiLocationException(
                        "Checked status not found.", LOCATOR_ALL_ENGINES, root);
            } else {
                return new Pair<String, Boolean>(engine, isChecked);
            }
        }
    }
    private EngineSelectionIndicator mEngineSelectionIndicator = new EngineSelectionIndicator();
    // The next 5 lines are boilerplate, no need to modify.
    private static final SearchEngineSelectionControllerForCodelab sInstance =
            new SearchEngineSelectionControllerForCodelab();
    private SearchEngineSelectionControllerForCodelab() {}
    public static SearchEngineSelectionControllerForCodelab getInstance() {
        return sInstance;
    }
    @Override
    public SearchEngineSelectionControllerForCodelab verifyActive() {
        // TODO: Implement this method.
        mLocatorHelper.verifyOnScreen(LOCATOR_SEARCH_ENGINE);
        return this;
    }
    /**
     * Choose the Omnibox default search engine.
     * @param   engine The engine to choose.
     */
    public SearchEngineSelectionControllerForCodelab chooseSearchEngine(String engineName) {
        // TODO: Perform a click on the engine choice.
        mUtils.click(getEngineLocator(engineName));
        // Returning this instead of void facilitates chaining.
        return this;
    }
    /**
     * @return The current engine choice.
     */
    public String getEngineChoice() {
        // TODO: Determine which engine option is selected and return it.
        String engineSelection = null;
        List<Pair<String, Boolean>> engineSelections =
                mLocatorHelper.getCustomElements(LOCATOR_ALL_ENGINES, mEngineSelectionIndicator);
        for (Pair<String, Boolean> pair : engineSelections) {
            if (pair.second) {
                if (engineSelection == null) {
                    engineSelection = pair.first;
                } else {
                    throw new UiLocationException("More than 1 engine chosen: " + engineSelection
                            + "," + pair.first + "!");
                }
            }
        }
        if (engineSelection == null) {
            throw new UiLocationException("No engines chosen!");
        }
        return engineSelection;
    }
    /**
     * @return IUi2Locator to find the engine choice.  Since the engine name
     *         varies across languages, this couldn't be hard-coded.
     */
    private IUi2Locator getEngineLocator(String engineName) {
        return Ui2Locators.withPath(LOCATOR_ALL_ENGINES, Ui2Locators.withText(engineName));
    }
}
```

4) controllers/ntp/ChromeMenu.java, add lines marked with "+":
``` java
...

+ import org.chromium.chrome.test.pagecontroller.controllers.codelab.SettingsControllerForCodelab;
...
public class ChromeMenu extends PageController {
...
+    private static final IUi2Locator LOCATOR_SETTINGS_FOR_CODELAB =
+            Ui2Locators.withPath(Ui2Locators.withAnyResEntry(R.id.menu_item_text),
+                    Ui2Locators.withTextString(R.string.menu_settings));
...
+    public SettingsControllerForCodelab openSettingsForCodelab() {
+        mUtils.click(LOCATOR_SETTINGS_FOR_CODELAB);
+        return SettingsControllerForCodelab.getInstance().verifyActive();
+    }
...
}
```

5) Add lines marked with '+' to chrome/test/android/BUILD.gn:
```
android_library("chrome_java_test_pagecontroller") {
  testonly = true
  sources = [
...
    "javatests/src/org/chromium/chrome/test/pagecontroller/controllers/android/PermissionDialog.java",
+    "javatests/src/org/chromium/chrome/test/pagecontroller/controllers/codelab/SettingsControllerForCodelab.java",
+    "javatests/src/org/chromium/chrome/test/pagecontroller/controllers/codelab/SearchEngineSelectionControllerForCodelab.java",
    "javatests/src/org/chromium/chrome/test/pagecontroller/controllers/first_run/TOSController.java",
...

```

6) Build and run.
``` bash
autoninja -C out/Debug chrome_java_test_pagecontroller_codelab

out/Debug/bin/run_chrome_java_test_pagecontroller_codelab --num-retries 0 --local-output
```
