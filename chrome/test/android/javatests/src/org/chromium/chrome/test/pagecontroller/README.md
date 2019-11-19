# Chrome's Page Controller Test Library

## Introduction

Page Controllers simplify the task of writing and maintaining integration tests
by organizing the logic to interact with the various application UI components
into re-usable classes.  Learn how to write and use Page Controllers to solve
your testing needs.


## File Organization

[codelabsolution](codelabsolution]: Solutions for the [code lab](#code-lab).
[controllers](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/controllers/): Contains all the Page Controllers.<br/>
[rules](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/rules/): Junit Test Rules that provide access to the Page Controllers in a
test case.<br/>
[tests](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/tests/): Tests for the Page Controllers themselves.<br/>
[utils](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/utils/): Utility classes that are useful for writing Page Controllers.<br/>


## Using IDEs
IDEs will make writing tests and maintaining Page Controllers much easier with online documentation and code completion.  See the [setup
guide](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/README.md#integrated-development-environment-ide_set-up-guides).


## Code Lab
The code lab is an introduction to writing new Page Controllers and using them in tests.

1) Create a new branch to go through the exercise, ex:
```
git checkout -b pagecontroller_codelab
```

2) Add code lab files to the build by adding the following lines to chrome/test/android/BUILD.gn.
```
...
android_library("chrome_java_test_pagecontroller") {
  testonly = true
  java_files = [
  ...
    "javatests/src/org/chromium/chrome/test/pagecontroller/controllers/android/PermissionDialog.java",
+   "javatests/src/org/chromium/chrome/test/pagecontroller/controllers/codelab/SearchEngineSelectionControllerForCodelab.java",
+   "javatests/src/org/chromium/chrome/test/pagecontroller/controllers/codelab/SettingsControllerForCodelab.java",
    "javatests/src/org/chromium/chrome/test/pagecontroller/controllers/first_run/DataSaverController.java",
  ...
  ]
```

3) Modify the following files according to TODO hints contained therein.  It's
best to build and test frequently iteratively as you complete the TODO items.
- Start here: [SettingsForCodelabTest.java](tests/codelab/SettingsForCodelabTest.java)
- Complete TODOs in: [controllers/ntp/ChromeMenu.java](controllers/ntp/ChromeMenu.java)
  (Hint, Read the ["Where to find the resource and string
  ids"](#where-to-find-the-resource-and-string-ids) section to find resource and
  string ids to use in locators.)
- Complete TODOs to pass SettingsForCodelabTest.testOpenCodelabSettings: [controllers/codelab/SettingsControllerForCodelab.java](controllers/codelab/SettingsControllerForCodelab.java)
- Complete TODOs to pass SettingsForCodelabTest.testSwitchSearchEngine:
  [controllers/codelab/SearchEngineSelectionControllerForCodelab.java](controllers/codelab/SearchEngineSelectionControllerForCodelab.java)

4) Build the codelab.
```
autoninja -C out/Debug chrome_java_test_pagecontroller_codelab
```

5) Run codelab tests (repeat from step 3 until all the tests pass).
```
# Open the html results link to view logcat to debug errors.  There will be an
# xml hierarchy dump of the UI in case of test failures in the logcat.
# Screenshot is also available in the test results page.

# Run the tests one at a time (to aid in troubleshooting) by using the -f option:
out/Debug/bin/run_chrome_java_test_pagecontroller_codelab --num-retries 0 --local-output -f "*SettingsForCodelabTest.testOpenSettings"

out/Debug/bin/run_chrome_java_test_pagecontroller_codelab --num-retries 0 --local-output -f "*SettingsForCodelabTest.testSwitchSearchEngine"
```

6) Sample answers are [here](codelabsolution/README.md) in case you get stuck.


## Writing Testcases

See the [ExampleTest](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/tests/ExampleTest.java).

## Creating + Updating Page Controllers

Currently the Page Controllers use UIAutomator under-the-hood.  But it is
entirely possible to write methods or whole page controllers using Espresso or
some other driver framework.

```
/**
 * Page controller for CoolNewPage
 */
class CoolNewPageController extends PageController {
    // Locators allow the controller to find UI elements on the page
    // It is preferred to use Resource Ids to find elements since they are
    // stable across minor UI changes.
    private static final IUi2Locator LOCATOR_COOL_PAGE = Ui2Locators.withAnyResEntry(R.id.cool_page);
    // Any of the resource ids in the list will result in a match.
    private static final IUi2Locator LOCATOR_COOL_BUTTON = Ui2Locators.withAnyResEntry(R.id.cool_button_v1, R.id.cool_button_v2);

    public CoolerPageController clickButton() {
        // [UiAutomatorUtils.click](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/utils/UiAutomatorUtils.java?q=click) operates on UI elements via IUi2Locators.
        // In general, methods that operate on IUi2Locators will throw if that
        // locator could not find an UI elements on the page.
        // UiAutomatorUtils has retry functionality with a configurable timeout,
        // so that flakiness can be drastically reduced.
        mUtils.click(LOCATOR_SOME_BUTTON);

        // If clicking on cool button should always result in the app going
        // to the EvenCoolerPage, then the action method should return an
        // instance of that controller via its assertIsCurrentPage() method.
        // This ensures the UI state is synced upon the return of the method.
        return EvenCoolerPageController.getInstance().assertIsCurrentPage();
    }

    // All page controllers must implement this method.
    @Override
    public SomePageController assertIsCurrentPage() {
        mLocatorHelper.get(LOCATOR_SOME_PAGE);  // Throws if not found
        return this;
    }
}
```

## Where to find the resource and string ids.

If you're working on a new UI change or are familiar with the UI that you want
to implement a Page Controller for, then you already should know what R.id.* and
R.string.* entries to use in the Page Controllers.  Prefer to construct locators
with these resources, for example:

```
import org.chromium.chrome.R;
...
IUi2Locator LOCATOR_TAB_SWITCHER_BUTTON = Ui2Locators.withResEntries(R.id.tab_switcher_button);

IUi2Locator LOCATOR_NEW_TAB_MENU_ITEM =
Ui2Locators.withPath(Ui2Locators.withResEntries(R.id.menu_item_text),
                     Ui2Locators.withText(R.string.menu_new_tab));
```

**It is highly recommended that unique R.id entries are present for important UI elements in an activity, add if they're missing.**

**It is also highly recommended that R.string.\* entries be used in Page Controllers and tests instead of hard-coding them.  This will keep tests in sync with the code-base and allow them to work under different language settings.**

If you are not too sure about the correct resource / string ids to use, see if you can find the developer or maintainer for the UI.  If they're not available, here are some tips:

 - [Android Layout inspector](https://developer.android.com/studio/debug/layout-inspector): This is useful to find out the resource ids (click an element, then expand the properties list, under mID).  To see strings, expand the text folder, see the mText field.  Note that this is the literal string, not the string resource id (see strings grd file below).

 - The res directory: Search in [chrome/android/java/res/](https://cs.chromium.org/chromium/src/chrome/android/java/res/) directory for resource ids and string ids.  They are defined there and then auto generated (@+id/xyz -> R.id.xyz)

 - Search the strings grd file with
   [search_strings.py](https://cs.chromium.org/chromium/src/tools/android/pagecontroller/search_strings.py).

   [android_chrome_strings.grd](https://cs.chromium.org/chromium/src/chrome/android/java/strings/android_chrome_strings.grd) contains the list of strings used in Clank.
   The entries are transformed into android strings using the name attribute by dropping the IDS_ prefix and converting the rest into lower case.  For example: IDS_MENU_BOOKMARKS -> R.string.menu_bookmarks.  There may be several string matching what's displayed but with different ids, be sure to read the "desc" field in the grd file and also the java source code for the UI to pick the right one.

 - The Clank java source code: [src/chrome/android/java/src/org/chromium/chrome/browser/](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/)

 Search for ids and strings discovered in the previous steps to see how they are used.
