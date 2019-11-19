#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from google.appengine.ext import webapp
from google.appengine.ext.webapp import util
from google.appengine.api import users
from google.appengine.api import urlfetch
from google.appengine.ext.webapp import template
from google.appengine.api.urlfetch import DownloadError
import oauth2
import urllib
import logging
import os
import time
import json

# Configuration

CONFIG = {
  'oauth_consumer_key': 'anonymous',
  'oauth_consumer_secret': 'anonymous',
  'license_server': 'https://www.googleapis.com',
  'license_path': '%(server)s/chromewebstore/v1/licenses/%(appid)s/%(userid)s',
  'oauth_token': 'INSERT OAUTH TOKEN HERE',
  'oauth_token_secret': 'INSERT OAUTH TOKEN SECRET HERE',
  'app_id': 'INSERT APPLICATION ID HERE',
}

# Check to see if the server has been deployed.  In the dev server, this
# env variable will start with 'Development', in production, it will start with
# 'Google App Engine'
IS_PRODUCTION = os.environ['SERVER_SOFTWARE'].startswith('Google App Engine')

# Valid access levels that may be returned by the license server.
VALID_ACCESS_LEVELS = ['FREE_TRIAL', 'FULL']


def fetch_license_data(userid):
  """Fetches the license for a given user by making an OAuth signed request
  to the license server.

  Args:
    userid OpenID of the user you are checking access for.

  Returns:
    The server's response as text.
  """
  url = CONFIG['license_path'] % {
    'server': CONFIG['license_server'],
    'appid': CONFIG['app_id'],
    'userid': urllib.quote_plus(userid),
  }

  oauth_token = oauth2.Token(**{
    'key': CONFIG['oauth_token'],
    'secret': CONFIG['oauth_token_secret']
  })

  oauth_consumer = oauth2.Consumer(**{
    'key': CONFIG['oauth_consumer_key'],
    'secret': CONFIG['oauth_consumer_secret']
  })

  logging.debug('Requesting %s' % url)
  client = oauth2.Client(oauth_consumer, oauth_token)
  resp, content = client.request(url, 'GET')
  logging.debug('Got response code %s, content %s' % (resp, content))
  return content


def parse_license_data(userid):
  """Returns the license for a given user as a structured object.

  Args:
    userid: The OpenID of the user to check.

  Returns:
    An object with the following parameters:
      error:  True if something went wrong, False otherwise.
      message: A descriptive message if error is True.
      access: One of 'NO', 'FREE_TRIAL', or 'FULL' depending on the access.
  """
  license = {'error': False, 'message': '', 'access': 'NO'}
  try:
    response_text = fetch_license_data(userid)
    try:
      logging.debug('Attempting to JSON parse: %s' % response_text)
      response = json.loads(response_text)
      logging.debug('Got license server response: %s' % response)
    except ValueError:
      logging.exception('Could not parse response as JSON: %s' % response_text)
      license['error'] = True
      license['message'] = 'Could not parse the license server response'
  except DownloadError:
    logging.exception('Could not fetch license data')
    license['error'] = True
    license['message'] = 'Could not fetch license data'

  if 'error' in response:
    license['error'] = True
    license['message'] = response['error']['message']
  elif (response['result'] == 'YES'
        and response['accessLevel'] in VALID_ACCESS_LEVELS):
    license['access'] = response['accessLevel']

  return license


class MainHandler(webapp.RequestHandler):
  """Request handler class."""
  def get(self):
    """Handler for GET requests."""
    user = users.get_current_user()
    if user:
      if IS_PRODUCTION:
        # We should use federated_identity in production, since the license
        # server requires an OpenID
        userid = user.federated_identity()
      else:
        # On the dev server, we won't have access to federated_identity, so
        # just use a default OpenID which will never return YES.
        # If you want to test different response values on the development
        # server, just change this default value (e.g. append '-yes' or
        # '-trial').
        userid = ('https://www.google.com/accounts/o8/id?'
                  'id=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx')
      license_data = parse_license_data(userid)
      template_data = {
        'license': license_data,
        'user_name': user.nickname(),
        'user_id': userid,
        'user_logout': users.create_logout_url(self.request.uri),
      }
    else:
      # Force the OpenID login endpoint to be for Google accounts only, since
      # the license server doesn't support any other type of OpenID provider.
      login_url = users.create_login_url(dest_url='/',
                      federated_identity='google.com/accounts/o8/id')
      template_data = {
        'user_login': login_url,
      }

    # Render a simple template
    path = os.path.join(os.path.dirname(__file__), 'templates', 'index.html')
    self.response.out.write(template.render(path, template_data))


if __name__ == '__main__':
  application = webapp.WSGIApplication([
    ('/', MainHandler),
  ], debug=False)
  util.run_wsgi_app(application)
