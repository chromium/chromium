/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The "Hello world!" of the Chrome Web Store Licensing API, in Java. This
 * program logs the user in with OpenID, fetches their license state with OAuth,
 * and prints one of these greetings as appropriate:
 *
 *   1. Hello *no* license!
 *   2. Hello *free trial* license!
 *   3. Hello *full* license!
 *
 * Brian Kennish <bkennish@chromium.org>
 */
package com.example;

import com.google.appengine.api.users.*;
import com.google.appengine.repackaged.org.json.JSONObject;

import java.io.*;
import java.net.*;
import java.util.HashSet;

import javax.servlet.http.*;

import oauth.signpost.OAuthConsumer;
import oauth.signpost.basic.DefaultOAuthConsumer;

/* A Google App Engine servlet. */
@SuppressWarnings("serial")
public class HelloLicenseServlet extends HttpServlet {
  /* TODO: The app ID from the Chrome Developer Dashboard. */
  public static final String APP_ID = "[INSERT APP ID HERE]";

  /* TODO: The token from the Chrome Developer Dashboard. */
  private static final String TOKEN = "[INSERT TOKEN HERE]";

  /* TODO: The token secret from the Chrome Developer Dashboard. */
  private static final String TOKEN_SECRET = "[INSERT TOKEN SECRET HERE]";

  /*
   * The license server URL, where %s are placeholders for app and
   * user IDs
   */
  public static final String SERVER_URL =
      "https://www.googleapis.com/chromewebstore/v1/licenses/%s/%s";

  /* The consumer key. */
  public static final String CONSUMER_KEY = "anonymous";

  /* The consumer secret. */
  public static final String CONSUMER_SECRET = CONSUMER_KEY;

  /* Handles "GET" requests. */
  public void doGet(HttpServletRequest request, HttpServletResponse response)
      throws IOException {
    response.setContentType("text/html; charset=UTF-8");
    UserService userService = UserServiceFactory.getUserService();
    PrintWriter output = response.getWriter();
    String url = request.getRequestURI();

    if (userService.isUserLoggedIn()) {
      // Provide a logout path.
      User user = userService.getCurrentUser();
      output.printf(
        "<strong>%s</strong> | <a href=\"%s\">Sign out</a><br><br>",
        user.getEmail(),
        userService.createLogoutURL(url)
      );

      try {
        // Send a signed request for the user's license state.
        OAuthConsumer oauth =
            new DefaultOAuthConsumer(CONSUMER_KEY, CONSUMER_SECRET);
        oauth.setTokenWithSecret(TOKEN, TOKEN_SECRET);
        URLConnection http =
            new URL(
              String.format(
                SERVER_URL,
                APP_ID,
                URLEncoder.encode(user.getFederatedIdentity(), "UTF-8")
              )
            ).openConnection();
        oauth.sign(http);
        http.connect();

        // Convert the response from the license server to a string.
        BufferedReader input =
            new BufferedReader(new InputStreamReader(http.getInputStream()));
        String file = "";
        for (String line; (line = input.readLine()) != null; file += line);
        input.close();

        // Parse the string as JSON and display the license state.
        JSONObject json = new JSONObject(file);
        output.printf(
          "Hello <strong>%s</strong> license!",
          "YES".equals(json.get("result")) ?
              "FULL".equals(json.get("accessLevel")) ? "full" : "free trial" :
              "no"
        );
      } catch (Exception exception) {
        // Dump any error.
        output.printf("Oops! <strong>%s</strong>", exception.getMessage());
      }
    } else { // The user isn't logged in.
      // Prompt for login.
      output.printf(
        "<a href=\"%s\">Sign in</a>",
        userService.createLoginURL(
          url,
          null,
          "https://www.google.com/accounts/o8/id",
          new HashSet<String>()
        )
      );
    }
  }
}
