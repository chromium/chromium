package org.chromium.chrome.test.transit.dom_distiller;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;

import java.util.concurrent.TimeoutException;

/** Conditions checking a Tab's WebContents for properties changed by Reader Mode. */
public class ReaderModeConditions {

    /** Checks the background color of the WebContents. */
    public static class TabBackgroundColorCondition extends Condition {

        private final String mExpectedColorString;
        private final Tab mTab;

        public TabBackgroundColorCondition(Tab tab, String colorString) {
            super(/* isRunOnUiThread= */ false);
            mTab = tab;
            mExpectedColorString = colorString;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws Exception {
            String query = "window.getComputedStyle(document.body)['backgroundColor']";
            String actualColorString = runJavaScript(mTab, query);
            return whether(actualColorString.equals(mExpectedColorString), actualColorString);
        }

        @Override
        public String buildDescription() {
            return "Page's background color is " + mExpectedColorString;
        }
    }

    /** Checks the font size of the WebContents. */
    public static class TabFontSizeCondition extends Condition {

        private final String mExpectedFontSizeString;
        private final Tab mTab;

        public TabFontSizeCondition(Tab tab, String fontSizeString) {
            super(/* isRunOnUiThread= */ false);
            mTab = tab;
            mExpectedFontSizeString = fontSizeString;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws Exception {
            String query = "window.getComputedStyle(document.body)['fontSize']";
            String actualColorString = runJavaScript(mTab, query);
            return whether(actualColorString.equals(mExpectedFontSizeString), actualColorString);
        }

        @Override
        public String buildDescription() {
            return "Page's background color is " + mExpectedFontSizeString;
        }
    }

    /**
     * Run JavaScript on a certain {@link Tab}.
     *
     * @param tab The tab to be injected to.
     * @param javaScript The JavaScript code to be injected.
     * @return The result of the code.
     */
    private static String runJavaScript(Tab tab, String javaScript) throws TimeoutException {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(tab.getWebContents(), javaScript);
        javascriptHelper.waitUntilHasValue();
        return javascriptHelper.getJsonResultAndClear();
    }
}
